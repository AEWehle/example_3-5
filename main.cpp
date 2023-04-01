/**
 * @brief   Programa de trabajo practico 3
 * @author  Angeles Wehle
 * @author  Axel Metzinger
 * 
 * probamos cambiando el lm35 por el pote y tambien vimos distintos tipos de print para la uart
 */

// formas de usar print en uart
// potentiometerReading = potentiometer.read(); //guardo un valor
// sprintf ( str, "alguna frase %.2f\r\n", potentiometerReading ); // guardo en str la frase con el valor
// stringLength = strlen(str); // largo
// uartUsb.write( str, stringLength ); // escrbo en uart
// //otra forma
// uartUsb.write( "Press '2' to get the gas detector state\r\n", 41 );  // ñastring y el largo
// // para leer
// uartUsb.readable() // devuelve true si hay entrada
// uartUsb.read( &receivedChar, 1 ); // guarda en putero receivedChar la cantidad de caracteres 1


//=====[Libraries]=============================================================

#include "mbed.h"
#include "arm_book_lib.h"
#include <cstring>
//=====[Defines]===============================================================

#define NUMBER_OF_KEYS                           4
#define BLINKING_TIME_GAS_ALARM               1000
#define BLINKING_TIME_OVER_TEMP_ALARM          500
#define BLINKING_TIME_GAS_AND_OVER_TEMP_ALARM  100
#define NUMBER_OF_AVG_SAMPLES                   100
#define OVER_TEMP_LEVEL                         30 // cuando el conversorad llegue a este valor sonará la alarma,
// nos ayudamos con el pote para elegir a que temperatura llegar
#define TIME_INCREMENT_MS                       10

//=====[Declaration and initialization of public global objects]===============

DigitalIn enterButton(BUTTON1); // boton azul de la placa
DigitalIn alarmTestButton(D2); // en este caos no lo vamos a usar, ya que usmaos el lm35
DigitalIn aButton(D4); // botones de la alarma 1100
DigitalIn bButton(D5);
DigitalIn cButton(D6);
DigitalIn dButton(D7);
DigitalIn mq2(PE_12); // detector de gas, en este caso no lo usamos
//D39 = PE_12, definido en pinnames.h, 
//mbed-os/targets/TARGET_STM/TARGET_STM32F4/TARGET_STM32F429xI/TARGET_NUCLEO_F429ZI/PinNames.h

DigitalOut alarmLed(LED1);
DigitalOut incorrectCodeLed(LED3); // cuando ponemos mal la alrma
DigitalOut systemBlockedLed(LED2); // cuando lo ponemos mal 5 veces

DigitalInOut sirenPin(PE_10); //D40 //no lo usamos aca

UnbufferedSerial uartUsb(USBTX, USBRX, 115200);

AnalogIn potentiometer(A0); // pote
AnalogIn lm35(A1); // detector de temperatura

//=====[Declaration and initialization of public global variables]=============

bool alarmState    = OFF;
bool incorrectCode = false;
bool overTempDetector = OFF;

int numberOfIncorrectCodes = 0;
int buttonBeingCompared    = 0;
int codeSequence[NUMBER_OF_KEYS]   = { 1, 1, 0, 0 };
int buttonsPressed[NUMBER_OF_KEYS] = { 0, 0, 0, 0 };
int accumulatedTimeAlarm = 0;

bool gasDetectorState          = OFF;
bool overTempDetectorState     = OFF;

float potentiometerReading = 0.0;
float lm35ReadingsAverage  = 0.0;
float lm35ReadingsSum      = 0.0;
float lm35ReadingsArray[NUMBER_OF_AVG_SAMPLES];
float lm35TempC            = 0.0;

//=====[Declarations (prototypes) of public functions]=========================

/**
 * Inicializar el estado de las variables de entrada
 */
void inputsInit();

/**
 * Inicializar el estado de las variables de salida
 */
void outputsInit();


/**
 * Actualización del estado de alarma en función del estado de los detectores
 */
void alarmActivationUpdate();

/**
 * Desactivación de la alarma si el código introducido es correcto
 */
void alarmDeactivationUpdate();


/**
 * Permite al usuario interactuar con el programa a través del puerto serie (UART)
 */
void uartTask();

/**
 * Muestra la lista de comandos disponibles en el puerto serie (UART)
 */
void availableCommands();

/**
 * Compara si el código introducido y el código esperado coinciden
 * @return  true si ambos códigos son iguales y false en caso contrario
 */
bool areEqual();

/**
 * Convierte una temperatura dada en Celsius a Fahrenheit
 * @param   tempInCelciusDegrees    temperatura a convertir
 * @return  la temperatura convertida en Fahrenheit
 */
float celsiusToFahrenheit( float tempInCelsiusDegrees );

/**
 * Convierte la tensión leída en la entrada analógica en una temperatura en grados Celsius
 * @param   analogReading   tension en la entrada analogica
 * @return  la temperatura en grados Celsius
 */
float analogReadingScaledWithTheLM35Formula( float analogReading );

//=====[Main function, the program entry point after power on or reset]========

/**
 * Función principal del programa.
 */
int main()
{
    inputsInit();
    outputsInit();
    while (true) {
        alarmActivationUpdate();
        alarmDeactivationUpdate();
        uartTask();
        delay(TIME_INCREMENT_MS);
    }
}

//=====[Implementations of public functions]===================================

void inputsInit()
{
    alarmTestButton.mode(PullDown);
    aButton.mode(PullDown);
    bButton.mode(PullDown);
    cButton.mode(PullDown);
    dButton.mode(PullDown);
    sirenPin.mode(OpenDrain); // no sonando
    sirenPin.input(); // para que no suene
    // para que suene hay que
    // sirenPin.output();                                     
    // sirenPin = LOW;   
}

void outputsInit()
{
    alarmLed = OFF;
    incorrectCodeLed = OFF;
    systemBlockedLed = OFF;
}

void alarmActivationUpdate()
{
    static int lm35SampleIndex = 0;
    int i = 0;

    lm35ReadingsArray[lm35SampleIndex] = lm35.read();
    lm35SampleIndex++;
    if ( lm35SampleIndex >= NUMBER_OF_AVG_SAMPLES) {
        lm35SampleIndex = 0;
    }
    
       lm35ReadingsSum = 0.0;
    for (i = 0; i < NUMBER_OF_AVG_SAMPLES; i++) {
        lm35ReadingsSum = lm35ReadingsSum + lm35ReadingsArray[i];
    }
    lm35ReadingsAverage = lm35ReadingsSum / NUMBER_OF_AVG_SAMPLES;
       lm35TempC = analogReadingScaledWithTheLM35Formula ( lm35ReadingsAverage );    
    
    if ( lm35TempC > OVER_TEMP_LEVEL ) {
        overTempDetector = ON;
    } else {
        overTempDetector = OFF;
    }

    if( !mq2) {
        gasDetectorState = ON;
        alarmState = ON;
    }
    if( overTempDetector ) {
        overTempDetectorState = ON;
        alarmState = ON;
    }
    if( alarmTestButton ) {             
        overTempDetectorState = ON;
        gasDetectorState = ON;
        alarmState = ON;
    }    
    if( alarmState ) { 
        accumulatedTimeAlarm = accumulatedTimeAlarm + TIME_INCREMENT_MS;
        sirenPin.output();                                     
        sirenPin = LOW;                                        
    
        if( gasDetectorState && overTempDetectorState ) {
            if( accumulatedTimeAlarm >= BLINKING_TIME_GAS_AND_OVER_TEMP_ALARM ) {
                accumulatedTimeAlarm = 0;
                alarmLed = !alarmLed;
            }
        } else if( gasDetectorState ) {
            if( accumulatedTimeAlarm >= BLINKING_TIME_GAS_ALARM ) {
                accumulatedTimeAlarm = 0;
                alarmLed = !alarmLed;
            }
        } else if ( overTempDetectorState ) {
            if( accumulatedTimeAlarm >= BLINKING_TIME_OVER_TEMP_ALARM  ) {
                accumulatedTimeAlarm = 0;
                alarmLed = !alarmLed;
            }
        }
    } else{
        alarmLed = OFF;
        gasDetectorState = OFF;
        overTempDetectorState = OFF;
        sirenPin.input();                                  
    }
}

void alarmDeactivationUpdate()
{
    if ( numberOfIncorrectCodes < 5 ) {
        if ( aButton && bButton && cButton && dButton && !enterButton ) {
            incorrectCodeLed = OFF;
        }
        if ( enterButton && !incorrectCodeLed && alarmState ) {
            buttonsPressed[0] = aButton;
            buttonsPressed[1] = bButton;
            buttonsPressed[2] = cButton;
            buttonsPressed[3] = dButton;
            if ( areEqual() ) {
                alarmState = OFF;
                numberOfIncorrectCodes = 0;
            } else {
                incorrectCodeLed = ON;
                numberOfIncorrectCodes++;
            }
        }
    } else {
        systemBlockedLed = ON;
    }
}

void uartTask()
{
    char receivedChar = '\0';
    char str[100];
    int stringLength;
    if( uartUsb.readable() ) {
        uartUsb.read( &receivedChar, 1 );
        switch (receivedChar) {
        case '1':
            if ( alarmState ) {
                uartUsb.write( "The alarm is activated\r\n", 24);
            } else {
                uartUsb.write( "The alarm is not activated\r\n", 28);
            }
            break;

        case '2':
            if ( !mq2 ) {
                uartUsb.write( "Gas is being detected\r\n", 22);
            } else {
                uartUsb.write( "Gas is not being detected\r\n", 27);
            }
            break;

        case '3':
            if ( overTempDetector ) {
                uartUsb.write( "Temperature is above the maximum level\r\n", 40);
            } else {
                uartUsb.write( "Temperature is below the maximum level\r\n", 40);
            }
            break;
            
        case '4':
            uartUsb.write( "Please enter the code sequence.\r\n", 33 );
            uartUsb.write( "First enter 'A', then 'B', then 'C', and ", 41 ); 
            uartUsb.write( "finally 'D' button\r\n", 20 );
            uartUsb.write( "In each case type 1 for pressed or 0 for ", 41 );
            uartUsb.write( "not pressed\r\n", 13 );
            uartUsb.write( "For example, for 'A' = pressed, ", 32 );
            uartUsb.write( "'B' = pressed, 'C' = not pressed, ", 34);
            uartUsb.write( "'D' = not pressed, enter '1', then '1', ", 40 );
            uartUsb.write( "then '0', and finally '0'\r\n\r\n", 29 );

            incorrectCode = false;

            for ( buttonBeingCompared = 0;
                  buttonBeingCompared < NUMBER_OF_KEYS;
                  buttonBeingCompared++) {

                uartUsb.read( &receivedChar, 1 );
                uartUsb.write( "*", 1 );

                if ( receivedChar == '1' ) {
                    if ( codeSequence[buttonBeingCompared] != 1 ) {
                        incorrectCode = true;
                    }
                } else if ( receivedChar == '0' ) {
                    if ( codeSequence[buttonBeingCompared] != 0 ) {
                        incorrectCode = true;
                    }
                } else {
                    incorrectCode = true;
                }
            }

            if ( incorrectCode == false ) {
                uartUsb.write( "\r\nThe code is correct\r\n\r\n", 25 );
                alarmState = OFF;
                incorrectCodeLed = OFF;
                numberOfIncorrectCodes = 0;
            } else {
                uartUsb.write( "\r\nThe code is incorrect\r\n\r\n", 27 );
                incorrectCodeLed = ON;
                numberOfIncorrectCodes++;
            }                
            break;

        case '5':
            uartUsb.write( "Please enter new code sequence\r\n", 32 );
            uartUsb.write( "First enter 'A', then 'B', then 'C', and ", 41 );
            uartUsb.write( "finally 'D' button\r\n", 20 );
            uartUsb.write( "In each case type 1 for pressed or 0 for not ", 45 );
            uartUsb.write( "pressed\r\n", 9 );
            uartUsb.write( "For example, for 'A' = pressed, 'B' = pressed,", 46 );
            uartUsb.write( " 'C' = not pressed,", 19 );
            uartUsb.write( "'D' = not pressed, enter '1', then '1', ", 40 );
            uartUsb.write( "then '0', and finally '0'\r\n\r\n", 29 );

            for ( buttonBeingCompared = 0; 
                  buttonBeingCompared < NUMBER_OF_KEYS; 
                  buttonBeingCompared++) {

                uartUsb.read( &receivedChar, 1 );
                uartUsb.write( "*", 1 );

                if ( receivedChar == '1' ) {
                    codeSequence[buttonBeingCompared] = 1;
                } else if ( receivedChar == '0' ) {
                    codeSequence[buttonBeingCompared] = 0;
                }
            }

            uartUsb.write( "\r\nNew code generated\r\n\r\n", 24 );
            break;
 
        case 'p':
        case 'P':
            potentiometerReading = potentiometer.read();
            sprintf ( str, "Potentiometer: %.2f\r\n", potentiometerReading );
            stringLength = strlen(str);
            uartUsb.write( str, stringLength );
            break;

        case 'c':
        case 'C':
            sprintf ( str, "Temperature: %.2f \xB0 C\r\n", lm35TempC );
            stringLength = strlen(str);
            uartUsb.write( str, stringLength );
            break;

        case 'f':
        case 'F':
            sprintf ( str, "Temperature: %.2f \xB0 F\r\n", 
                celsiusToFahrenheit( lm35TempC ) );
            stringLength = strlen(str);
            uartUsb.write( str, stringLength );
            break;

        default:
            availableCommands();
            break;

        }
    }
}

void availableCommands()
{
    uartUsb.write( "Available commands:\r\n", 21 );
    uartUsb.write( "Press '1' to get the alarm state\r\n", 34 );
    uartUsb.write( "Press '2' to get the gas detector state\r\n", 41 );
    uartUsb.write( "Press '3' to get the over temperature detector state\r\n", 54 );
    uartUsb.write( "Press '4' to enter the code sequence\r\n", 38 );
    uartUsb.write( "Press '5' to enter a new code\r\n", 31 );
    uartUsb.write( "Press 'P' or 'p' to get potentiometer reading\r\n", 47 );
    uartUsb.write( "Press 'f' or 'F' to get lm35 reading in Fahrenheit\r\n", 52 );
    uartUsb.write( "Press 'c' or 'C' to get lm35 reading in Celsius\r\n\r\n", 51 );
}

bool areEqual()
{
    int i;

    for (i = 0; i < NUMBER_OF_KEYS; i++) {
        if (codeSequence[i] != buttonsPressed[i]) {
            return false;
        }
    }

    return true;
}

float analogReadingScaledWithTheLM35Formula( float analogReading )
{// como usamos 50 la tempmeratura seria 16500
    return ( analogReading * 3.3 / 0.01 );
}

float celsiusToFahrenheit( float tempInCelsiusDegrees )
{
    return ( tempInCelsiusDegrees * 9.0 / 5.0 + 32.0 );
}
