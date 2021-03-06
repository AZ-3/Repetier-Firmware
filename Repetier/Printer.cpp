/*
    This file is part of the Repetier-Firmware for RF devices from Conrad Electronic SE.

    Repetier-Firmware is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Repetier-Firmware is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Repetier-Firmware.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "Repetier.h"
#include <Wire.h>

#if USE_ADVANCE
uint8_t         Printer::maxExtruderSpeed;                              ///< Timer delay for end extruder speed
volatile int    Printer::extruderStepsNeeded;                           ///< This many extruder steps are still needed, <0 = reverse steps needed.
#endif // USE_ADVANCE

uint8_t         Printer::unitIsInches = 0;                              ///< 0 = Units are mm, 1 = units are inches.

//Stepper Movement Variables
float           Printer::axisStepsPerMM[4] = {XAXIS_STEPS_PER_MM,YAXIS_STEPS_PER_MM,ZAXIS_STEPS_PER_MM,1}; ///< Number of steps per mm needed.
float           Printer::invAxisStepsPerMM[4];                          ///< Inverse of axisStepsPerMM for faster conversion
float           Printer::maxFeedrate[4] = {MAX_FEEDRATE_X, MAX_FEEDRATE_Y, MAX_FEEDRATE_Z, DIRECT_FEEDRATE_E}; ///< Maximum allowed feedrate. //DIRECT_FEEDRATE_E added by nibbels, wird aber überschrieben.
float           Printer::homingFeedrate[3];

#ifdef RAMP_ACCELERATION
float           Printer::maxAccelerationMMPerSquareSecond[4] = {MAX_ACCELERATION_UNITS_PER_SQ_SECOND_X,MAX_ACCELERATION_UNITS_PER_SQ_SECOND_Y,MAX_ACCELERATION_UNITS_PER_SQ_SECOND_Z}; ///< X, Y, Z and E max acceleration in mm/s^2 for printing moves or retracts
float           Printer::maxTravelAccelerationMMPerSquareSecond[4] = {MAX_TRAVEL_ACCELERATION_UNITS_PER_SQ_SECOND_X,MAX_TRAVEL_ACCELERATION_UNITS_PER_SQ_SECOND_Y,MAX_TRAVEL_ACCELERATION_UNITS_PER_SQ_SECOND_Z}; ///< X, Y, Z max acceleration in mm/s^2 for travel moves
#if FEATURE_MILLING_MODE
short           Printer::max_milling_all_axis_acceleration = MILLER_ACCELERATION; //miller min speed is limited to too high speed because of acceleration-formula. We use this value in milling mode and make it adjustable within small numbers like 5 to 100 or something like that.
#endif // FEATURE_MILLING_MODE

/** Acceleration in steps/s^2 in printing mode.*/
unsigned long   Printer::maxPrintAccelerationStepsPerSquareSecond[4];
/** Acceleration in steps/s^2 in movement mode.*/
unsigned long   Printer::maxTravelAccelerationStepsPerSquareSecond[4];
uint32_t        Printer::maxInterval;
#endif // RAMP_ACCELERATION

uint8_t         Printer::relativeCoordinateMode = false;                ///< Determines absolute (false) or relative Coordinates (true).
uint8_t         Printer::relativeExtruderCoordinateMode = false;        ///< Determines Absolute or Relative E Codes while in Absolute Coordinates mode. E is always relative in Relative Coordinates mode.

volatile long   Printer::queuePositionLastSteps[4];
volatile float  Printer::queuePositionLastMM[3];
volatile float  Printer::queuePositionCommandMM[3];
volatile long   Printer::queuePositionTargetSteps[4];
float           Printer::originOffsetMM[3] = {0,0,0};
uint8_t         Printer::flag0 = 0;
uint8_t         Printer::flag1 = 0;
uint8_t         Printer::flag2 = 0;
uint8_t         Printer::flag3 = 0;

#if ALLOW_EXTENDED_COMMUNICATION < 2
uint8_t         Printer::debugLevel = 0; ///< Bitfield defining debug output. 1 = echo, 2 = info, 4 = error, 8 = dry run., 16 = Only communication, 32 = No moves
#else
uint8_t         Printer::debugLevel = 6; ///< Bitfield defining debug output. 1 = echo, 2 = info, 4 = error, 8 = dry run., 16 = Only communication, 32 = No moves
#endif // ALLOW_EXTENDED_COMMUNICATION < 2

uint8_t         Printer::stepsPerTimerCall = 1;
uint16_t        Printer::stepsDoublerFrequency = STEP_DOUBLER_FREQUENCY;
uint8_t         Printer::menuMode = 0;

unsigned long   Printer::interval;                                      ///< Last step duration in ticks.
unsigned long   Printer::timer;                                         ///< used for acceleration/deceleration timing
unsigned long   Printer::stepNumber;                                    ///< Step number in current move.

#if USE_ADVANCE
#ifdef ENABLE_QUADRATIC_ADVANCE
long            Printer::advanceExecuted;                               ///< Executed advance steps
#endif // ENABLE_QUADRATIC_ADVANCE

volatile int    Printer::advanceStepsSet;
#endif // USE_ADVANCE

//float         Printer::minimumSpeed;                                  ///< lowest allowed speed to keep integration error small
//float         Printer::minimumZSpeed;
long            Printer::maxSteps[3];                                   ///< For software endstops, limit of move in positive direction.
long            Printer::minSteps[3];                                   ///< For software endstops, limit of move in negative direction.
float           Printer::lengthMM[3];
float           Printer::minMM[3];
float           Printer::feedrate;                                      ///< Last requested feedrate.
int             Printer::feedrateMultiply;                              ///< Multiplier for feedrate in percent (factor 1 = 100)
int             Printer::extrudeMultiply;                               ///< Flow multiplier in percdent (factor 1 = 100)
float           Printer::extrudeMultiplyError = 0;
float           Printer::extrusionFactor = 1.0;
float           Printer::maxJerk;                                       ///< Maximum allowed jerk in mm/s
float           Printer::maxZJerk;                                      ///< Maximum allowed jerk in z direction in mm/s
float           Printer::extruderOffset[3];                             ///< offset for different extruder positions.
unsigned int    Printer::vMaxReached;                                   ///< Maximum reached speed
unsigned long   Printer::msecondsPrinting;                              ///< Milliseconds of printing time (means time with heated extruder)
unsigned long   Printer::msecondsMilling;                               ///< Milliseconds of milling time
float           Printer::filamentPrinted;                               ///< mm of filament printed since counting started
long            Printer::ZOffset;                                       ///< Z Offset in um
char            Printer::ZMode               = DEFAULT_Z_SCALE_MODE;    ///< Z Scale  1 = show the z-distance to z-min (print) or to the z-origin (mill), 2 = show the z-distance to the surface of the heat bed (print) or work part (mill)
char            Printer::moveMode[3];                                   ///< move mode which is applied within the Position X/Y/Z menus

#if ENABLE_BACKLASH_COMPENSATION
float           Printer::backlash[3];
uint8_t         Printer::backlashDir;
#endif // ENABLE_BACKLASH_COMPENSATION

#if FEATURE_MEMORY_POSITION
float           Printer::memoryX;
float           Printer::memoryY;
float           Printer::memoryZ;
float           Printer::memoryE;
#endif // FEATURE_MEMORY_POSITION

#ifdef DEBUG_PRINT
int             debugWaitLoop = 0;
#endif // DEBUG_PRINT

#if FEATURE_HEAT_BED_Z_COMPENSATION
volatile char   Printer::doHeatBedZCompensation;
#endif // FEATURE_HEAT_BED_Z_COMPENSATION

#if FEATURE_WORK_PART_Z_COMPENSATION
volatile char   Printer::doWorkPartZCompensation;
volatile long   Printer::staticCompensationZ;
#endif // FEATURE_WORK_PART_Z_COMPENSATION

volatile long   Printer::queuePositionCurrentSteps[3];
volatile char   Printer::stepperDirection[3];
volatile char   Printer::blockAll;

#if FEATURE_Z_MIN_OVERRIDE_VIA_GCODE
volatile long   Printer::currentZSteps;
#endif // FEATURE_Z_MIN_OVERRIDE_VIA_GCODE

#if FEATURE_HEAT_BED_Z_COMPENSATION || FEATURE_WORK_PART_Z_COMPENSATION
volatile long   Printer::compensatedPositionTargetStepsZ;
volatile long   Printer::compensatedPositionCurrentStepsZ;

volatile char   Printer::endZCompensationStep;
#endif // FEATURE_HEAT_BED_Z_COMPENSATION || FEATURE_WORK_PART_Z_COMPENSATION

#if FEATURE_EXTENDED_BUTTONS || FEATURE_PAUSE_PRINTING
volatile long   Printer::directPositionTargetSteps[4];
volatile long   Printer::directPositionCurrentSteps[4];
long            Printer::directPositionLastSteps[4];
char            Printer::waitMove;
#endif // FEATURE_EXTENDED_BUTTONS || FEATURE_PAUSE_PRINTING

#if FEATURE_MILLING_MODE
char            Printer::operatingMode;
float           Printer::drillFeedrate;
float           Printer::drillZDepth;
#endif // FEATURE_MILLING_MODE

#if FEATURE_CONFIGURABLE_Z_ENDSTOPS
char            Printer::ZEndstopType;
char            Printer::ZEndstopUnknown;
char            Printer::lastZDirection;
char            Printer::endstopZMinHit;
char            Printer::endstopZMaxHit;
#endif // FEATURE_CONFIGURABLE_Z_ENDSTOPS

#if FEATURE_CONFIGURABLE_MILLER_TYPE
char            Printer::MillerType;
#endif // FEATURE_CONFIGURABLE_MILLER_TYPE

#if STEPPER_ON_DELAY
char            Printer::enabledStepper[3];
#endif // STEPPER_ON_DELAY

#if FEATURE_BEEPER
char            Printer::enableBeeper;
#endif // FEATURE_BEEPER

#if FEATURE_CASE_LIGHT
char            Printer::enableCaseLight;
#endif // FEATURE_CASE_LIGHT

#if FEATURE_RGB_LIGHT_EFFECTS
char            Printer::RGBLightMode;
char            Printer::RGBLightStatus;
unsigned long   Printer::RGBLightIdleStart;
char            Printer::RGBButtonBackPressed;
char            Printer::RGBLightModeForceWhite;
#endif // FEATURE_RGB_LIGHT_EFFECTS

#if FEATURE_230V_OUTPUT
char            Printer::enable230VOutput;
#endif // FEATURE_230V_OUTPUT

#if FEATURE_24V_FET_OUTPUTS
char            Printer::enableFET1;
char            Printer::enableFET2;
char            Printer::enableFET3;
#endif // FEATURE_24V_FET_OUTPUTS

#if FEATURE_CASE_FAN
bool            Printer::ignoreFanOn = false;
unsigned long   Printer::prepareFanOff = 0;
unsigned long   Printer::fanOffDelay = 0;
#endif // FEATURE_CASE_FAN

#if FEATURE_TYPE_EEPROM
unsigned char   Printer::wrongType;
#endif // FEATURE_TYPE_EEPROM

#if FEATURE_UNLOCK_MOVEMENT
//When the printer resets, we should not do movement, because it would not be homed. At least some interaction with buttons or temperature-commands are needed to allow movement.
unsigned char   Printer::g_unlock_movement = 0;
#endif //FEATURE_UNLOCK_MOVEMENT

uint8_t         Printer::motorCurrent[5] = {0,0,0,0,0};

#if FEATURE_ZERO_DIGITS
bool            Printer::g_pressure_offset_active = true;
short           Printer::g_pressure_offset = 0;
#endif // FEATURE_ZERO_DIGITS

void Printer::constrainQueueDestinationCoords()
{
    if(isNoDestinationCheck()) return;

#if FEATURE_EXTENDED_BUTTONS || FEATURE_PAUSE_PRINTING
#if max_software_endstop_x == true
    if (queuePositionTargetSteps[X_AXIS] + directPositionTargetSteps[X_AXIS] > Printer::maxSteps[X_AXIS]) Printer::queuePositionTargetSteps[X_AXIS] = Printer::maxSteps[X_AXIS] - directPositionTargetSteps[X_AXIS];
#endif // max_software_endstop_x == true

#if max_software_endstop_y == true
    if (queuePositionTargetSteps[Y_AXIS] + directPositionTargetSteps[Y_AXIS] > Printer::maxSteps[Y_AXIS]) Printer::queuePositionTargetSteps[Y_AXIS] = Printer::maxSteps[Y_AXIS] - directPositionTargetSteps[Y_AXIS];
#endif // max_software_endstop_y == true

#if max_software_endstop_z == true
    if (queuePositionTargetSteps[Z_AXIS] + directPositionTargetSteps[Z_AXIS] > Printer::maxSteps[Z_AXIS]) Printer::queuePositionTargetSteps[Z_AXIS] = Printer::maxSteps[Z_AXIS] - directPositionTargetSteps[Z_AXIS];
#endif // max_software_endstop_z == true
#else
#if max_software_endstop_x == true
    if (queuePositionTargetSteps[X_AXIS] > Printer::maxSteps[X_AXIS]) Printer::queuePositionTargetSteps[X_AXIS] = Printer::maxSteps[X_AXIS];
#endif // max_software_endstop_x == true

#if max_software_endstop_y == true
    if (queuePositionTargetSteps[Y_AXIS] > Printer::maxSteps[Y_AXIS]) Printer::queuePositionTargetSteps[Y_AXIS] = Printer::maxSteps[Y_AXIS];
#endif // max_software_endstop_y == true

#if max_software_endstop_z == true
    if (queuePositionTargetSteps[Z_AXIS] > Printer::maxSteps[Z_AXIS]) Printer::queuePositionTargetSteps[Z_AXIS] = Printer::maxSteps[Z_AXIS];
#endif // max_software_endstop_z == true
#endif //FEATURE_EXTENDED_BUTTONS || FEATURE_PAUSE_PRINTING

} // constrainQueueDestinationCoords


void Printer::constrainDirectDestinationCoords()
{
    if(isNoDestinationCheck()) return;
    if(g_pauseStatus) return; //pausebewegung rechnet mit current queue
#if FEATURE_EXTENDED_BUTTONS || FEATURE_PAUSE_PRINTING
#if max_software_endstop_x == true
    if (queuePositionTargetSteps[X_AXIS] + directPositionTargetSteps[X_AXIS] > Printer::maxSteps[X_AXIS]) Printer::directPositionTargetSteps[X_AXIS] = Printer::maxSteps[X_AXIS] - queuePositionTargetSteps[X_AXIS];
#endif // max_software_endstop_x == true

#if max_software_endstop_y == true
    if (queuePositionTargetSteps[Y_AXIS] + directPositionTargetSteps[Y_AXIS] > Printer::maxSteps[Y_AXIS]) Printer::directPositionTargetSteps[Y_AXIS] = Printer::maxSteps[Y_AXIS] - queuePositionTargetSteps[Y_AXIS];
#endif // max_software_endstop_y == true

#if max_software_endstop_z == true
    if (queuePositionTargetSteps[Z_AXIS] + directPositionTargetSteps[Z_AXIS] > Printer::maxSteps[Z_AXIS]) Printer::directPositionTargetSteps[Z_AXIS] = Printer::maxSteps[Z_AXIS] - queuePositionTargetSteps[Z_AXIS];
#endif // max_software_endstop_z == true
#endif //FEATURE_EXTENDED_BUTTONS || FEATURE_PAUSE_PRINTING
} // constrainDirectDestinationCoords


bool Printer::isPositionAllowed(float x,float y,float z)
{
    if(isNoDestinationCheck())  return true;
    bool allowed = true;
    //Nibbels 11.01.17: Die Funktion ist so wie sie ist etwas unnötig und prüft nix... blende warnings aus.
    (void)x;
    (void)y;
    (void)z;
    
    if(!allowed)
    {
        Printer::updateCurrentPosition(true);
        Commands::printCurrentPosition();
    }
    return allowed;

} // isPositionAllowed


void Printer::updateDerivedParameter()
{
    maxSteps[X_AXIS] = (long)(axisStepsPerMM[X_AXIS]*(minMM[X_AXIS]+lengthMM[X_AXIS]));
    maxSteps[Y_AXIS] = (long)(axisStepsPerMM[Y_AXIS]*(minMM[Y_AXIS]+lengthMM[Y_AXIS]));
    maxSteps[Z_AXIS] = (long)(axisStepsPerMM[Z_AXIS]*(minMM[Z_AXIS]+lengthMM[Z_AXIS]));
    minSteps[X_AXIS] = (long)(axisStepsPerMM[X_AXIS]*minMM[X_AXIS]);
    minSteps[Y_AXIS] = (long)(axisStepsPerMM[Y_AXIS]*minMM[Y_AXIS]);
    minSteps[Z_AXIS] = (long)(axisStepsPerMM[Z_AXIS]*minMM[Z_AXIS]);

    // For which directions do we need backlash compensation
#if ENABLE_BACKLASH_COMPENSATION
    backlashDir &= 7;
    if(backlashX!=0) backlashDir |= 8;
    if(backlashY!=0) backlashDir |= 16;
    if(backlashZ!=0) backlashDir |= 32;
#endif // ENABLE_BACKLASH_COMPENSATION

    for(uint8_t i = 0; i < 4; i++)
    {
        invAxisStepsPerMM[i] = 1.0f / axisStepsPerMM[i];

#ifdef RAMP_ACCELERATION
#if FEATURE_MILLING_MODE
        if( Printer::operatingMode == OPERATING_MODE_PRINT )
        {
#endif // FEATURE_MILLING_MODE
            /** Acceleration in steps/s^2 in printing mode.*/
            maxPrintAccelerationStepsPerSquareSecond[i] = maxAccelerationMMPerSquareSecond[i] * axisStepsPerMM[i];
            /** Acceleration in steps/s^2 in movement mode.*/
            maxTravelAccelerationStepsPerSquareSecond[i] = maxTravelAccelerationMMPerSquareSecond[i] * axisStepsPerMM[i];
#if FEATURE_MILLING_MODE
        }
        else
        {
            /** Acceleration in steps/s^2 in milling mode.*/
            maxPrintAccelerationStepsPerSquareSecond[i] = MILLER_ACCELERATION * axisStepsPerMM[i];
            /** Acceleration in steps/s^2 in milling-movement mode.*/
            maxTravelAccelerationStepsPerSquareSecond[i] = MILLER_ACCELERATION * axisStepsPerMM[i];
        }
#endif  // FEATURE_MILLING_MODE
#endif // RAMP_ACCELERATION
    } 
    
    float accel;
#if FEATURE_MILLING_MODE
    if( Printer::operatingMode == OPERATING_MODE_PRINT )
    {
#endif // FEATURE_MILLING_MODE
      accel = RMath::max(maxAccelerationMMPerSquareSecond[X_AXIS], maxTravelAccelerationMMPerSquareSecond[X_AXIS]);
#if FEATURE_MILLING_MODE
    }
    else{
      accel = MILLER_ACCELERATION;
    }
#endif  // FEATURE_MILLING_MODE
    
    float minimumSpeed = accel * sqrt(2.0f / (axisStepsPerMM[X_AXIS] * accel));
    if(maxJerk < 2 * minimumSpeed) {// Enforce minimum start speed if target is faster and jerk too low
        maxJerk = 2 * minimumSpeed;
        Com::printFLN(PSTR("XY jerk was too low, setting to "), maxJerk);
    }
    
    
    
#if FEATURE_MILLING_MODE
    if( Printer::operatingMode == OPERATING_MODE_PRINT )
    {
#endif // FEATURE_MILLING_MODE
      accel = RMath::max(maxAccelerationMMPerSquareSecond[Z_AXIS], maxTravelAccelerationMMPerSquareSecond[Z_AXIS]);
#if FEATURE_MILLING_MODE
    }
#endif  // FEATURE_MILLING_MODE
    float minimumZSpeed = 0.5 * accel * sqrt(2.0f / (axisStepsPerMM[Z_AXIS] * accel));
    if(maxZJerk < 2 * minimumZSpeed) {
        maxZJerk = 2 * minimumZSpeed;
        Com::printFLN(PSTR("Z jerk was too low, setting to "), maxZJerk);
    }
    
    maxInterval = F_CPU / (minimumSpeed * axisStepsPerMM[X_AXIS]);
    uint32_t tmp = F_CPU / (minimumSpeed * axisStepsPerMM[Y_AXIS]);
    if(tmp < maxInterval)
        maxInterval = tmp;
    tmp = F_CPU / (minimumZSpeed * axisStepsPerMM[Z_AXIS]);
    if(tmp < maxInterval) 
        maxInterval = tmp;

    Printer::updateAdvanceFlags();

} // updateDerivedParameter


/** \brief Stop heater and stepper motors. Disable power,if possible. */
void Printer::kill(uint8_t only_steppers)
{
    if(areAllSteppersDisabled() && only_steppers) return;
    if(Printer::isAllKilled()) return;

#if FAN_PIN>-1 && FEATURE_FAN_CONTROL
    // disable the fan
    Commands::setFanSpeed(0,false);
#endif // FAN_PIN>-1 && FEATURE_FAN_CONTROL

    setAllSteppersDisabled();
    disableXStepper();
    disableYStepper();
    disableZStepper();
    Extruder::disableAllExtruders();

#if FAN_BOARD_PIN>-1
    pwm_pos[NUM_EXTRUDER+1] = 0;
#endif // FAN_BOARD_PIN

    if(!only_steppers)
    {
        for(uint8_t i=0; i<NUM_TEMPERATURE_LOOPS; i++)
            Extruder::setTemperatureForExtruder(0,i);
        Extruder::setHeatedBedTemperature(0);
        UI_STATUS_UPD(UI_TEXT_KILLED);

#if defined(PS_ON_PIN) && PS_ON_PIN>-1
        //pinMode(PS_ON_PIN,INPUT);
        SET_OUTPUT(PS_ON_PIN); //GND
        WRITE(PS_ON_PIN, (POWER_INVERTING ? LOW : HIGH));
#endif // defined(PS_ON_PIN) && PS_ON_PIN>-1

        Printer::setAllKilled(true);
    }
    else
    {
        UI_STATUS_UPD(UI_TEXT_STEPPER_DISABLED);
    }

} // kill


void Printer::updateAdvanceFlags()
{
    Printer::setAdvanceActivated(false);

#if USE_ADVANCE
    for(uint8_t i = 0; i < NUM_EXTRUDER; i++) {
        if(extruder[i].advanceL!=0) {
            Printer::setAdvanceActivated(true);
        }
#ifdef ENABLE_QUADRATIC_ADVANCE
        if(extruder[i].advanceK != 0) Printer::setAdvanceActivated(true);
#endif // ENABLE_QUADRATIC_ADVANCE
    }
#endif // USE_ADVANCE

} // updateAdvanceFlags


// This is for untransformed move to coordinates in printers absolute Cartesian space
void Printer::moveTo(float x,float y,float z,float e,float f)
{
    if(x != IGNORE_COORDINATE)
        queuePositionTargetSteps[X_AXIS] = (x + Printer::extruderOffset[X_AXIS]) * axisStepsPerMM[X_AXIS];
    if(y != IGNORE_COORDINATE)
        queuePositionTargetSteps[Y_AXIS] = (y + Printer::extruderOffset[Y_AXIS]) * axisStepsPerMM[Y_AXIS];
    if(z != IGNORE_COORDINATE)
        queuePositionTargetSteps[Z_AXIS] = (z + Printer::extruderOffset[Z_AXIS]) * axisStepsPerMM[Z_AXIS];
    if(e != IGNORE_COORDINATE)
        queuePositionTargetSteps[E_AXIS] = e * axisStepsPerMM[E_AXIS];
    if(f != IGNORE_COORDINATE)
        feedrate = f;

    PrintLine::prepareQueueMove(ALWAYS_CHECK_ENDSTOPS,true);
    updateCurrentPosition(false);

} // moveTo

/** Move to transformed Cartesian coordinates, mapping real (model) space to printer space.
*/
void Printer::moveToReal(float x,float y,float z,float e,float f)
{
    if(x == IGNORE_COORDINATE)        x = queuePositionLastMM[X_AXIS];
    else queuePositionLastMM[X_AXIS] = x;
    if(y == IGNORE_COORDINATE)        y = queuePositionLastMM[Y_AXIS];
    else queuePositionLastMM[Y_AXIS] = y;
    if(z == IGNORE_COORDINATE)        z = queuePositionLastMM[Z_AXIS];
    else queuePositionLastMM[Z_AXIS] = z;

    x += Printer::extruderOffset[X_AXIS];
    y += Printer::extruderOffset[Y_AXIS];
    z += Printer::extruderOffset[Z_AXIS];

    queuePositionTargetSteps[X_AXIS] = static_cast<int32_t>(floor(x * axisStepsPerMM[X_AXIS] + 0.5));
    queuePositionTargetSteps[Y_AXIS] = static_cast<int32_t>(floor(y * axisStepsPerMM[Y_AXIS] + 0.5));
    queuePositionTargetSteps[Z_AXIS] = static_cast<int32_t>(floor(z * axisStepsPerMM[Z_AXIS] + 0.5));
    if(e != IGNORE_COORDINATE)
        queuePositionTargetSteps[E_AXIS] = e * axisStepsPerMM[E_AXIS];
    if(f != IGNORE_COORDINATE)
        feedrate = f;

    PrintLine::prepareQueueMove(ALWAYS_CHECK_ENDSTOPS,true);

} // moveToReal


uint8_t Printer::setOrigin(float xOff,float yOff,float zOff)
{
    if( !areAxisHomed() )
    {
        if( debugErrors() )
        {
            // we can not set the origin when we do not know the home position
            Com::printFLN( PSTR("setOrigin(): home position is unknown") );
        }

        showError( (void*)ui_text_set_origin, (void*)ui_text_home_unknown );
        return 0;
    }

    originOffsetMM[X_AXIS] = xOff;
    originOffsetMM[Y_AXIS] = yOff;
    originOffsetMM[Z_AXIS] = zOff;

    if( debugInfo() )
    {
        // output the new origin offsets
        Com::printF( PSTR("setOrigin(): x="), originOffsetMM[X_AXIS] );
        Com::printF( PSTR("; y="), originOffsetMM[Y_AXIS] );
        Com::printFLN( PSTR("; z="), originOffsetMM[Z_AXIS] );
    }
    return 1;

} // setOrigin


void Printer::updateCurrentPosition(bool copyLastCmd)
{
    queuePositionLastMM[X_AXIS] = (float)(queuePositionLastSteps[X_AXIS])*invAxisStepsPerMM[X_AXIS];
    queuePositionLastMM[Y_AXIS] = (float)(queuePositionLastSteps[Y_AXIS])*invAxisStepsPerMM[Y_AXIS];
    queuePositionLastMM[Z_AXIS] = (float)(queuePositionLastSteps[Z_AXIS])*invAxisStepsPerMM[Z_AXIS];
    queuePositionLastMM[X_AXIS] -= Printer::extruderOffset[X_AXIS];
    queuePositionLastMM[Y_AXIS] -= Printer::extruderOffset[Y_AXIS];
    queuePositionLastMM[Z_AXIS] -= Printer::extruderOffset[Z_AXIS];

    if(copyLastCmd)
    {
        queuePositionCommandMM[X_AXIS] = queuePositionLastMM[X_AXIS];
        queuePositionCommandMM[Y_AXIS] = queuePositionLastMM[Y_AXIS];
        queuePositionCommandMM[Z_AXIS] = queuePositionLastMM[Z_AXIS];
    }

} // updateCurrentPosition


/**
  \brief Sets the destination coordinates to values stored in com.

  For the computation of the destination, the following facts are considered:
  - Are units inches or mm.
  - Relative or absolute positioning with special case only extruder relative.
  - Offset in x and y direction for multiple extruder support.
*/
uint8_t Printer::setDestinationStepsFromGCode(GCode *com)
{
    register long   p;
    float           x, y, z;

    if(!relativeCoordinateMode)
    {
        if(com->hasX()) queuePositionCommandMM[X_AXIS] = queuePositionLastMM[X_AXIS] = convertToMM(com->X) - originOffsetMM[X_AXIS];
        if(com->hasY()) queuePositionCommandMM[Y_AXIS] = queuePositionLastMM[Y_AXIS] = convertToMM(com->Y) - originOffsetMM[Y_AXIS];
        if(com->hasZ()) queuePositionCommandMM[Z_AXIS] = queuePositionLastMM[Z_AXIS] = convertToMM(com->Z) - originOffsetMM[Z_AXIS];
    }
    else
    {
        if(com->hasX()) queuePositionLastMM[X_AXIS] = (queuePositionCommandMM[X_AXIS] += convertToMM(com->X));
        if(com->hasY()) queuePositionLastMM[Y_AXIS] = (queuePositionCommandMM[Y_AXIS] += convertToMM(com->Y));
        if(com->hasZ()) queuePositionLastMM[Z_AXIS] = (queuePositionCommandMM[Z_AXIS] += convertToMM(com->Z));
    }

    x = queuePositionCommandMM[X_AXIS] + Printer::extruderOffset[X_AXIS];
    y = queuePositionCommandMM[Y_AXIS] + Printer::extruderOffset[Y_AXIS];
    z = queuePositionCommandMM[Z_AXIS] + Printer::extruderOffset[Z_AXIS];

    long xSteps = static_cast<long>(floor(x * axisStepsPerMM[X_AXIS] + 0.5f));
    long ySteps = static_cast<long>(floor(y * axisStepsPerMM[Y_AXIS] + 0.5f));
    long zSteps = static_cast<long>(floor(z * axisStepsPerMM[Z_AXIS] + 0.5f));
    
    if(com->hasX())
    {
        queuePositionTargetSteps[X_AXIS] = xSteps;
    }
    else
    {
        queuePositionTargetSteps[X_AXIS] = queuePositionLastSteps[X_AXIS];
    }

    if(com->hasY())
    {
        queuePositionTargetSteps[Y_AXIS] = ySteps;
    }
    else
    {
        queuePositionTargetSteps[Y_AXIS] = queuePositionLastSteps[Y_AXIS];
    }

    if(com->hasZ())
    {
        queuePositionTargetSteps[Z_AXIS] = zSteps;
    }
    else
    {
        queuePositionTargetSteps[Z_AXIS] = queuePositionLastSteps[Z_AXIS];
    }

    if(com->hasE() && !Printer::debugDryrun())
    {
        p = convertToMM(com->E * axisStepsPerMM[E_AXIS]);
        if(relativeCoordinateMode || relativeExtruderCoordinateMode)
        {
            if(
#if MIN_EXTRUDER_TEMP > 30
                Extruder::current->tempControl.currentTemperatureC < MIN_EXTRUDER_TEMP ||
#endif // MIN_EXTRUDER_TEMP > 30

                fabs(com->E) * extrusionFactor > EXTRUDE_MAXLENGTH)
                    {
                        p = 0;
                    }
            queuePositionTargetSteps[E_AXIS] = queuePositionLastSteps[E_AXIS] + p;
            
        }
        else
        {
            if(
#if MIN_EXTRUDER_TEMP > 30
                Extruder::current->tempControl.currentTemperatureC < MIN_EXTRUDER_TEMP ||
#endif // MIN_EXTRUDER_TEMP > 30
                fabs(p - queuePositionLastSteps[E_AXIS]) * extrusionFactor > EXTRUDE_MAXLENGTH * axisStepsPerMM[E_AXIS])
                    {
                        queuePositionLastSteps[E_AXIS] = p;
                    }
            queuePositionTargetSteps[E_AXIS] = p;
        }
    }
    else
    {
        queuePositionTargetSteps[E_AXIS] = queuePositionLastSteps[E_AXIS];
    }

    if(com->hasF())
    {
        if(unitIsInches)
            feedrate = com->F * (float)feedrateMultiply * 0.0042333f;  // Factor is 25.5/60/100
        else
            feedrate = com->F * (float)feedrateMultiply * 0.00016666666f;
    }

    if(!Printer::isPositionAllowed(x,y,z))
    {
        queuePositionLastSteps[E_AXIS] = queuePositionTargetSteps[E_AXIS];
        return false; // ignore move
    }
    return !com->hasNoXYZ() || (com->hasE() && queuePositionTargetSteps[E_AXIS] != queuePositionLastSteps[E_AXIS]); // ignore unproductive moves

} // setDestinationStepsFromGCode


/**
  \brief Sets the destination coordinates to the passed values.
*/
uint8_t Printer::setDestinationStepsFromMenu( float relativeX, float relativeY, float relativeZ )
{
    float   x, y, z;


    if( relativeX ) queuePositionLastMM[X_AXIS] = (queuePositionCommandMM[X_AXIS] += relativeX);
    if( relativeY ) queuePositionLastMM[Y_AXIS] = (queuePositionCommandMM[Y_AXIS] += relativeY);
    if( relativeZ ) queuePositionLastMM[Z_AXIS] = (queuePositionCommandMM[Z_AXIS] += relativeZ);

    x = queuePositionCommandMM[X_AXIS] + Printer::extruderOffset[X_AXIS];
    y = queuePositionCommandMM[Y_AXIS] + Printer::extruderOffset[Y_AXIS];
    z = queuePositionCommandMM[Z_AXIS] + Printer::extruderOffset[Z_AXIS];

    long xSteps = static_cast<long>(floor(x * axisStepsPerMM[X_AXIS] + 0.5));
    long ySteps = static_cast<long>(floor(y * axisStepsPerMM[Y_AXIS] + 0.5));
    long zSteps = static_cast<long>(floor(z * axisStepsPerMM[Z_AXIS] + 0.5));
    
    if( relativeX )
    {
        queuePositionTargetSteps[X_AXIS] = xSteps;
    }
    else
    {
        queuePositionTargetSteps[X_AXIS] = queuePositionLastSteps[X_AXIS];
    }

    if( relativeY )
    {
        queuePositionTargetSteps[Y_AXIS] = ySteps;
    }
    else
    {
        queuePositionTargetSteps[Y_AXIS] = queuePositionLastSteps[Y_AXIS];
    }

    if( relativeZ )
    {
        queuePositionTargetSteps[Z_AXIS] = zSteps;
    }
    else
    {
        queuePositionTargetSteps[Z_AXIS] = queuePositionLastSteps[Z_AXIS];
    }

    if( !Printer::isPositionAllowed( x, y, z ) )
    {
        if( Printer::debugErrors() )
        {
            //Com::printFLN( PSTR( "We should not be here." ) );
        }
        return false; // ignore move
    }

    PrintLine::prepareQueueMove( ALWAYS_CHECK_ENDSTOPS, true );
    return true;

} // setDestinationStepsFromMenu


void Printer::setup()
{
    HAL::stopWatchdog();

    for(uint8_t i=0; i<NUM_EXTRUDER+3; i++) pwm_pos[i]=0;

#if FEATURE_MILLING_MODE
    if( Printer::operatingMode == OPERATING_MODE_PRINT )
    {
        Printer::homingFeedrate[X_AXIS] = HOMING_FEEDRATE_X_PRINT;
        Printer::homingFeedrate[Y_AXIS] = HOMING_FEEDRATE_Y_PRINT;
        Printer::homingFeedrate[Z_AXIS] = HOMING_FEEDRATE_Z_PRINT;
    }
    else
    {
        Printer::homingFeedrate[X_AXIS] = HOMING_FEEDRATE_X_MILL;
        Printer::homingFeedrate[Y_AXIS] = HOMING_FEEDRATE_Y_MILL;
        Printer::homingFeedrate[Z_AXIS] = HOMING_FEEDRATE_Z_MILL;
    }
#else
    Printer::homingFeedrate[X_AXIS] = HOMING_FEEDRATE_X_PRINT;
    Printer::homingFeedrate[Y_AXIS] = HOMING_FEEDRATE_Y_PRINT;
    Printer::homingFeedrate[Z_AXIS] = HOMING_FEEDRATE_Z_PRINT;
#endif // FEATURE_MILLING_MODE

    //HAL::delayMilliseconds(500);  // add a delay at startup to give hardware time for initalization
    HAL::hwSetup();

    HAL::allowInterrupts();

#if FEATURE_BEEPER
    enableBeeper = BEEPER_MODE;
#endif // FEATURE_BEEPER

    Wire.begin();

#if FEATURE_TYPE_EEPROM
    determineHardwareType();

    if( wrongType )
    {
        // this firmware is not for this hardware
        while( 1 )
        {
            // this firmware shall not try to do anything at this hardware
        }
    }
#endif // FEATURE_TYPE_EEPROM

#ifdef ANALYZER
// Channel->pin assignments
#if ANALYZER_CH0>=0
    SET_OUTPUT(ANALYZER_CH0);
#endif // ANALYZER_CH0>=0

#if ANALYZER_CH1>=0
    SET_OUTPUT(ANALYZER_CH1);
#endif // ANALYZER_CH1>=0

#if ANALYZER_CH2>=0
    SET_OUTPUT(ANALYZER_CH2);
#endif // ANALYZER_CH2>=0

#if ANALYZER_CH3>=0
    SET_OUTPUT(ANALYZER_CH3);
#endif // ANALYZER_CH3>=0

#if ANALYZER_CH4>=0
    SET_OUTPUT(ANALYZER_CH4);
#endif // ANALYZER_CH4>=0

#if ANALYZER_CH5>=0
    SET_OUTPUT(ANALYZER_CH5);
#endif // ANALYZER_CH5>=0

#if ANALYZER_CH6>=0
    SET_OUTPUT(ANALYZER_CH6);
#endif // ANALYZER_CH6>=0

#if ANALYZER_CH7>=0
    SET_OUTPUT(ANALYZER_CH7);
#endif // ANALYZER_CH7>=0
#endif // ANALYZER

#if defined(ENABLE_POWER_ON_STARTUP) && PS_ON_PIN>-1
    SET_OUTPUT(PS_ON_PIN); //GND
    WRITE(PS_ON_PIN, (POWER_INVERTING ? HIGH : LOW));
#endif // defined(ENABLE_POWER_ON_STARTUP) && PS_ON_PIN>-1

    //Initialize Step Pins
    SET_OUTPUT(X_STEP_PIN);
    SET_OUTPUT(Y_STEP_PIN);
    SET_OUTPUT(Z_STEP_PIN);

    //Initialize Dir Pins
#if X_DIR_PIN>-1
    SET_OUTPUT(X_DIR_PIN);
#endif // X_DIR_PIN>-1

#if Y_DIR_PIN>-1
    SET_OUTPUT(Y_DIR_PIN);
#endif // Y_DIR_PIN>-1

#if Z_DIR_PIN>-1
    SET_OUTPUT(Z_DIR_PIN);
#endif // Z_DIR_PIN>-1

    //Steppers default to disabled.
#if X_ENABLE_PIN > -1
    SET_OUTPUT(X_ENABLE_PIN);
    if(!X_ENABLE_ON) WRITE(X_ENABLE_PIN,HIGH);
    disableXStepper();
#endif // X_ENABLE_PIN > -1

#if Y_ENABLE_PIN > -1
    SET_OUTPUT(Y_ENABLE_PIN);
    if(!Y_ENABLE_ON) WRITE(Y_ENABLE_PIN,HIGH);
    disableYStepper();
#endif // Y_ENABLE_PIN > -1

#if Z_ENABLE_PIN > -1
    SET_OUTPUT(Z_ENABLE_PIN);
    if(!Z_ENABLE_ON) WRITE(Z_ENABLE_PIN,HIGH);
    disableZStepper();
#endif // Z_ENABLE_PIN > -1

#if FEATURE_TWO_XSTEPPER
    SET_OUTPUT(X2_STEP_PIN);
    SET_OUTPUT(X2_DIR_PIN);

#if X2_ENABLE_PIN > -1
    SET_OUTPUT(X2_ENABLE_PIN);
    if(!X_ENABLE_ON) WRITE(X2_ENABLE_PIN,HIGH);
#endif // X2_ENABLE_PIN > -1
#endif // FEATURE_TWO_XSTEPPER

#if FEATURE_TWO_YSTEPPER
    SET_OUTPUT(Y2_STEP_PIN);
    SET_OUTPUT(Y2_DIR_PIN);

#if Y2_ENABLE_PIN > -1
    SET_OUTPUT(Y2_ENABLE_PIN);
    if(!Y_ENABLE_ON) WRITE(Y2_ENABLE_PIN,HIGH);
#endif // Y2_ENABLE_PIN > -1
#endif // FEATURE_TWO_YSTEPPER

#if FEATURE_TWO_ZSTEPPER
    SET_OUTPUT(Z2_STEP_PIN);
    SET_OUTPUT(Z2_DIR_PIN);

#if X2_ENABLE_PIN > -1
    SET_OUTPUT(Z2_ENABLE_PIN);
    if(!Z_ENABLE_ON) WRITE(Z2_ENABLE_PIN,HIGH);
#endif // X2_ENABLE_PIN > -1
#endif // FEATURE_TWO_ZSTEPPER

    //endstop pullups
#if MIN_HARDWARE_ENDSTOP_X
#if X_MIN_PIN>-1
    SET_INPUT(X_MIN_PIN);

#if ENDSTOP_PULLUP_X_MIN
    PULLUP(X_MIN_PIN,HIGH);
#endif // ENDSTOP_PULLUP_X_MIN
#else
#error You have defined hardware x min endstop without pin assignment. Set pin number for X_MIN_PIN
#endif // X_MIN_PIN>-1
#endif // MIN_HARDWARE_ENDSTOP_X

#if MIN_HARDWARE_ENDSTOP_Y
#if Y_MIN_PIN>-1
    SET_INPUT(Y_MIN_PIN);

#if ENDSTOP_PULLUP_Y_MIN
    PULLUP(Y_MIN_PIN,HIGH);
#endif // ENDSTOP_PULLUP_Y_MIN
#else
#error You have defined hardware y min endstop without pin assignment. Set pin number for Y_MIN_PIN
#endif // Y_MIN_PIN>-1
#endif // MIN_HARDWARE_ENDSTOP_Y

#if MIN_HARDWARE_ENDSTOP_Z
#if Z_MIN_PIN>-1
    SET_INPUT(Z_MIN_PIN);

#if ENDSTOP_PULLUP_Z_MIN
    PULLUP(Z_MIN_PIN,HIGH);
#endif // ENDSTOP_PULLUP_Z_MIN
#else
#error You have defined hardware z min endstop without pin assignment. Set pin number for Z_MIN_PIN
#endif // Z_MIN_PIN>-1
#endif // MIN_HARDWARE_ENDSTOP_Z

#if MAX_HARDWARE_ENDSTOP_X
#if X_MAX_PIN>-1
    SET_INPUT(X_MAX_PIN);

#if ENDSTOP_PULLUP_X_MAX
    PULLUP(X_MAX_PIN,HIGH);
#endif // ENDSTOP_PULLUP_X_MAX
#else
#error You have defined hardware x max endstop without pin assignment. Set pin number for X_MAX_PIN
#endif // X_MAX_PIN>-1
#endif // MAX_HARDWARE_ENDSTOP_X

#if MAX_HARDWARE_ENDSTOP_Y
#if Y_MAX_PIN>-1
    SET_INPUT(Y_MAX_PIN);

#if ENDSTOP_PULLUP_Y_MAX
    PULLUP(Y_MAX_PIN,HIGH);
#endif // ENDSTOP_PULLUP_Y_MAX
#else
#error You have defined hardware y max endstop without pin assignment. Set pin number for Y_MAX_PIN
#endif // Y_MAX_PIN>-1
#endif // MAX_HARDWARE_ENDSTOP_Y

#if MAX_HARDWARE_ENDSTOP_Z
#if Z_MAX_PIN>-1
    SET_INPUT(Z_MAX_PIN);

#if ENDSTOP_PULLUP_Z_MAX
    PULLUP(Z_MAX_PIN,HIGH);
#endif // ENDSTOP_PULLUP_Z_MAX
#else
#error You have defined hardware z max endstop without pin assignment. Set pin number for Z_MAX_PIN
#endif // Z_MAX_PIN>-1
#endif // MAX_HARDWARE_ENDSTOP_Z

#if FEATURE_USER_INT3
    SET_INPUT( RESERVE_DIGITAL_PIN_PD3 );
    PULLUP( RESERVE_DIGITAL_PIN_PD3, HIGH );
    attachInterrupt( digitalPinToInterrupt(RESERVE_DIGITAL_PIN_PD3) , USER_INTERRUPT3_HOOK, FALLING );
#endif //FEATURE_USER_INT3

#if FEATURE_READ_CALIPER
// read for using pins : https://www.arduino.cc/en/Tutorial/DigitalPins
//where the clock comes in and triggers an interrupt which reads data then:
    SET_INPUT( FEATURE_READ_CALIPER_INT_PIN ); //input as default already this is here for explaination more than really having an input.
    PULLUP( FEATURE_READ_CALIPER_INT_PIN, HIGH ); //do I need this pullup??
    attachInterrupt( digitalPinToInterrupt(FEATURE_READ_CALIPER_INT_PIN) , FEATURE_READ_CALIPER_HOOK, FALLING );
//where data is to read when Int triggers because of clock from caliper:
    SET_INPUT( FEATURE_READ_CALIPER_DATA_PIN ); //input as default already this is here for explaination more than really having an input.
    PULLUP( FEATURE_READ_CALIPER_DATA_PIN, HIGH ); //do I need this pullup??
#endif //FEATURE_READ_CALIPER

#if FAN_PIN>-1 && FEATURE_FAN_CONTROL
    SET_OUTPUT(FAN_PIN);
    WRITE(FAN_PIN,LOW);
#endif // FAN_PIN>-1 && FEATURE_FAN_CONTROL

#if FAN_BOARD_PIN>-1
    SET_OUTPUT(FAN_BOARD_PIN);
    WRITE(FAN_BOARD_PIN,LOW);
#endif // FAN_BOARD_PIN>-1

#if EXT0_HEATER_PIN>-1
    SET_OUTPUT(EXT0_HEATER_PIN);
    WRITE(EXT0_HEATER_PIN,HEATER_PINS_INVERTED);
#endif // EXT0_HEATER_PIN>-1

#if defined(EXT1_HEATER_PIN) && EXT1_HEATER_PIN>-1 && NUM_EXTRUDER>1
    SET_OUTPUT(EXT1_HEATER_PIN);
    WRITE(EXT1_HEATER_PIN,HEATER_PINS_INVERTED);
#endif // defined(EXT1_HEATER_PIN) && EXT1_HEATER_PIN>-1 && NUM_EXTRUDER>1

#if defined(EXT2_HEATER_PIN) && EXT2_HEATER_PIN>-1 && NUM_EXTRUDER>2
    SET_OUTPUT(EXT2_HEATER_PIN);
    WRITE(EXT2_HEATER_PIN,HEATER_PINS_INVERTED);
#endif // defined(EXT2_HEATER_PIN) && EXT2_HEATER_PIN>-1 && NUM_EXTRUDER>2

#if defined(EXT3_HEATER_PIN) && EXT3_HEATER_PIN>-1 && NUM_EXTRUDER>3
    SET_OUTPUT(EXT3_HEATER_PIN);
    WRITE(EXT3_HEATER_PIN,HEATER_PINS_INVERTED);
#endif // defined(EXT3_HEATER_PIN) && EXT3_HEATER_PIN>-1 && NUM_EXTRUDER>3

#if defined(EXT4_HEATER_PIN) && EXT4_HEATER_PIN>-1 && NUM_EXTRUDER>4
    SET_OUTPUT(EXT4_HEATER_PIN);
    WRITE(EXT4_HEATER_PIN,HEATER_PINS_INVERTED);
#endif // defined(EXT4_HEATER_PIN) && EXT4_HEATER_PIN>-1 && NUM_EXTRUDER>4

#if defined(EXT5_HEATER_PIN) && EXT5_HEATER_PIN>-1 && NUM_EXTRUDER>5
    SET_OUTPUT(EXT5_HEATER_PIN);
    WRITE(EXT5_HEATER_PIN,HEATER_PINS_INVERTED);
#endif // defined(EXT5_HEATER_PIN) && EXT5_HEATER_PIN>-1 && NUM_EXTRUDER>5

#if EXT0_EXTRUDER_COOLER_PIN>-1
    SET_OUTPUT(EXT0_EXTRUDER_COOLER_PIN);
    WRITE(EXT0_EXTRUDER_COOLER_PIN,LOW);
#endif // EXT0_EXTRUDER_COOLER_PIN>-1

#if defined(EXT1_EXTRUDER_COOLER_PIN) && EXT1_EXTRUDER_COOLER_PIN>-1 && NUM_EXTRUDER>1
    SET_OUTPUT(EXT1_EXTRUDER_COOLER_PIN);
    WRITE(EXT1_EXTRUDER_COOLER_PIN,LOW);
#endif // defined(EXT1_EXTRUDER_COOLER_PIN) && EXT1_EXTRUDER_COOLER_PIN>-1 && NUM_EXTRUDER>1

#if defined(EXT2_EXTRUDER_COOLER_PIN) && EXT2_EXTRUDER_COOLER_PIN>-1 && NUM_EXTRUDER>2
    SET_OUTPUT(EXT2_EXTRUDER_COOLER_PIN);
    WRITE(EXT2_EXTRUDER_COOLER_PIN,LOW);
#endif // defined(EXT2_EXTRUDER_COOLER_PIN) && EXT2_EXTRUDER_COOLER_PIN>-1 && NUM_EXTRUDER>2

#if defined(EXT3_EXTRUDER_COOLER_PIN) && EXT3_EXTRUDER_COOLER_PIN>-1 && NUM_EXTRUDER>3
    SET_OUTPUT(EXT3_EXTRUDER_COOLER_PIN);
    WRITE(EXT3_EXTRUDER_COOLER_PIN,LOW);
#endif // defined(EXT3_EXTRUDER_COOLER_PIN) && EXT3_EXTRUDER_COOLER_PIN>-1 && NUM_EXTRUDER>3

#if defined(EXT4_EXTRUDER_COOLER_PIN) && EXT4_EXTRUDER_COOLER_PIN>-1 && NUM_EXTRUDER>4
    SET_OUTPUT(EXT4_EXTRUDER_COOLER_PIN);
    WRITE(EXT4_EXTRUDER_COOLER_PIN,LOW);
#endif // defined(EXT4_EXTRUDER_COOLER_PIN) && EXT4_EXTRUDER_COOLER_PIN>-1 && NUM_EXTRUDER>4

#if defined(EXT5_EXTRUDER_COOLER_PIN) && EXT5_EXTRUDER_COOLER_PIN>-1 && NUM_EXTRUDER>5
    SET_OUTPUT(EXT5_EXTRUDER_COOLER_PIN);
    WRITE(EXT5_EXTRUDER_COOLER_PIN,LOW);
#endif // defined(EXT5_EXTRUDER_COOLER_PIN) && EXT5_EXTRUDER_COOLER_PIN>-1 && NUM_EXTRUDER>5

    motorCurrentControlInit(); // Set current if it is firmware controlled

    feedrate = 50; ///< Current feedrate in mm/s.
    feedrateMultiply = 100;
    extrudeMultiply = 100;
    queuePositionCommandMM[X_AXIS] = queuePositionCommandMM[Y_AXIS] = queuePositionCommandMM[Z_AXIS] = 0;

#if USE_ADVANCE
#ifdef ENABLE_QUADRATIC_ADVANCE
    advanceExecuted = 0;
#endif // ENABLE_QUADRATIC_ADVANCE
    advanceStepsSet = 0;
#endif // USE_ADVANCE

    queuePositionLastSteps[X_AXIS] = queuePositionLastSteps[Y_AXIS] = queuePositionLastSteps[Z_AXIS] = queuePositionLastSteps[E_AXIS] = 0;

#if FEATURE_EXTENDED_BUTTONS || FEATURE_PAUSE_PRINTING
    directPositionTargetSteps[X_AXIS]  = directPositionTargetSteps[Y_AXIS]  = directPositionTargetSteps[Z_AXIS]  = directPositionTargetSteps[E_AXIS]  = 0;
    directPositionCurrentSteps[X_AXIS] = directPositionCurrentSteps[Y_AXIS] = directPositionCurrentSteps[Z_AXIS] = directPositionCurrentSteps[E_AXIS] = 0;
    directPositionLastSteps[X_AXIS]    = directPositionLastSteps[Y_AXIS]    = directPositionLastSteps[Z_AXIS]    = directPositionLastSteps[E_AXIS]    = 0;
    waitMove                           = 0;
#endif // FEATURE_EXTENDED_BUTTONS || FEATURE_PAUSE_PRINTING

    maxJerk = MAX_JERK;
    maxZJerk = MAX_ZJERK;
    extruderOffset[X_AXIS] = extruderOffset[Y_AXIS] = extruderOffset[Z_AXIS] = 0;

    ZOffset = 0;
    interval = 5000;
    stepsPerTimerCall = 1;
    msecondsPrinting = 0;
    msecondsMilling = 0;
    filamentPrinted = 0;
    flag0 = PRINTER_FLAG0_STEPPER_DISABLED;

    moveMode[X_AXIS] = DEFAULT_MOVE_MODE_X;
    moveMode[Y_AXIS] = DEFAULT_MOVE_MODE_Y;
    moveMode[Z_AXIS] = DEFAULT_MOVE_MODE_Z;

#if ENABLE_BACKLASH_COMPENSATION
    backlash[X_AXIS] = X_BACKLASH;
    backlash[Y_AXIS] = Y_BACKLASH;
    backlash[Z_AXIS] = Z_BACKLASH;
    backlashDir = 0;
#endif // ENABLE_BACKLASH_COMPENSATION

#if FEATURE_HEAT_BED_Z_COMPENSATION
    doHeatBedZCompensation = 0;
#endif // FEATURE_HEAT_BED_Z_COMPENSATION

#if FEATURE_WORK_PART_Z_COMPENSATION
    doWorkPartZCompensation = 0;
    staticCompensationZ     = 0;
#endif // FEATURE_WORK_PART_Z_COMPENSATION

    queuePositionCurrentSteps[X_AXIS] = 
    queuePositionCurrentSteps[Y_AXIS] = 
    queuePositionCurrentSteps[Z_AXIS] = 0;
    stepperDirection[X_AXIS]          = 
    stepperDirection[Y_AXIS]          = 
    stepperDirection[Z_AXIS]          = 0;
    blockAll                          = 0;

#if FEATURE_Z_MIN_OVERRIDE_VIA_GCODE
    currentZSteps                     = 0;
#endif // FEATURE_Z_MIN_OVERRIDE_VIA_GCODE

#if FEATURE_HEAT_BED_Z_COMPENSATION || FEATURE_WORK_PART_Z_COMPENSATION
    compensatedPositionTargetStepsZ  =
    compensatedPositionCurrentStepsZ = 0;
    endZCompensationStep             = 0;
#endif // FEATURE_HEAT_BED_Z_COMPENSATION || FEATURE_WORK_PART_Z_COMPENSATION

#if FEATURE_EXTENDED_BUTTONS || FEATURE_PAUSE_PRINTING
    directPositionCurrentSteps[X_AXIS] =
    directPositionCurrentSteps[Y_AXIS] =
    directPositionCurrentSteps[Z_AXIS] =
    directPositionCurrentSteps[E_AXIS] =
    directPositionTargetSteps[X_AXIS]  =
    directPositionTargetSteps[Y_AXIS]  =
    directPositionTargetSteps[Z_AXIS]  =
    directPositionTargetSteps[E_AXIS]  = 0;
#endif // FEATURE_EXTENDED_BUTTONS || FEATURE_PAUSE_PRINTING

#if FEATURE_MILLING_MODE
    operatingMode = DEFAULT_OPERATING_MODE;
    drillFeedrate = 0.0;
    drillZDepth   = 0.0;
#endif // FEATURE_MILLING_MODE

#if FEATURE_MILLING_MODE
    if( Printer::operatingMode == OPERATING_MODE_PRINT )
    {
        lengthMM[X_AXIS] = X_MAX_LENGTH_PRINT;
    }
    else
    {
        lengthMM[X_AXIS] = X_MAX_LENGTH_MILL;
    }
#else
    lengthMM[X_AXIS] = X_MAX_LENGTH_PRINT;
#endif // FEATURE_MILLING_MODE

    lengthMM[Y_AXIS] = Y_MAX_LENGTH;
    lengthMM[Z_AXIS] = Z_MAX_LENGTH;
    minMM[X_AXIS] = X_MIN_POS;
    minMM[Y_AXIS] = Y_MIN_POS;
    minMM[Z_AXIS] = Z_MIN_POS;

#if FEATURE_CONFIGURABLE_Z_ENDSTOPS
    ZEndstopType          = DEFAULT_Z_ENDSTOP_TYPE;
    ZEndstopUnknown       = 0;
    lastZDirection        = 0;
    endstopZMinHit        = ENDSTOP_NOT_HIT;
    endstopZMaxHit        = ENDSTOP_NOT_HIT;
#endif // FEATURE_CONFIGURABLE_Z_ENDSTOPS


#if STEPPER_ON_DELAY
    enabledStepper[X_AXIS] = 0;
    enabledStepper[Y_AXIS] = 0;
    enabledStepper[Z_AXIS] = 0;
#endif // STEPPER_ON_DELAY

#if FEATURE_CASE_LIGHT
    enableCaseLight = CASE_LIGHTS_DEFAULT_ON;
#endif // FEATURE_CASE_LIGHT

#if FEATURE_24V_FET_OUTPUTS
    enableFET1 = FET1_DEFAULT_ON;
    enableFET2 = FET2_DEFAULT_ON;
    enableFET3 = FET3_DEFAULT_ON;

    SET_OUTPUT(FET1);
    WRITE(FET1, enableFET1);
    
    SET_OUTPUT(FET2);
    WRITE(FET2, enableFET2);
    
    SET_OUTPUT(FET3);
    WRITE(FET3, enableFET3);
#endif // FEATURE_24V_FET_OUTPUTS

#if FEATURE_CASE_FAN
    fanOffDelay = CASE_FAN_OFF_DELAY;
#endif // FEATURE_CASE_FAN

#if FEATURE_RGB_LIGHT_EFFECTS
    RGBLightMode = RGB_LIGHT_DEFAULT_MODE;
    if( RGBLightMode == RGB_MODE_AUTOMATIC )
    {
        RGBLightStatus = RGB_STATUS_AUTOMATIC;
    }
    else
    {
        RGBLightStatus = RGB_STATUS_NOT_AUTOMATIC;
    }
    RGBLightIdleStart      = 0;
    RGBButtonBackPressed   = 0;
    RGBLightModeForceWhite = 0;
#endif // FEATURE_RGB_LIGHT_EFFECTS

#if USE_ADVANCE
    extruderStepsNeeded = 0;
#endif // USE_ADVANCE

    EEPROM::initBaudrate();
    HAL::serialSetBaudrate(baudrate);

    // sending of this information tells the Repetier-Host that the firmware has restarted - never delete or change this to-be-sent information
    Com::println();
    Com::printFLN(Com::tStart); //http://forum.repetier.com/discussion/comment/16949/#Comment_16949

    UI_INITIALIZE;

    HAL::showStartReason();
    Extruder::initExtruder();
    EEPROM::init(); // Read settings from eeprom if wanted

#if FEATURE_230V_OUTPUT
    enable230VOutput = OUTPUT_230V_DEFAULT_ON;
    SET_OUTPUT(OUTPUT_230V_PIN);
    WRITE(OUTPUT_230V_PIN, enable230VOutput);
#endif // FEATURE_230V_OUTPUT

#if FEATURE_CASE_LIGHT
    SET_OUTPUT(CASE_LIGHT_PIN);
    WRITE(CASE_LIGHT_PIN, enableCaseLight);
#endif // FEATURE_CASE_LIGHT

    SET_OUTPUT(CASE_FAN_PIN);

#if CASE_FAN_ALWAYS_ON
    WRITE(CASE_FAN_PIN, 1);
#else
    WRITE(CASE_FAN_PIN, 0);
#endif // CASE_FAN_ALWAYS_ON

    updateDerivedParameter();
    Commands::checkFreeMemory();
    Commands::writeLowestFreeRAM();
    HAL::setupTimer();

    Extruder::selectExtruderById(0);

#if SDSUPPORT
    sd.initsd();
#endif // SDSUPPORT

    g_nActiveHeatBed = (char)readWord24C256( I2C_ADDRESS_EXTERNAL_EEPROM, EEPROM_OFFSET_ACTIVE_HEAT_BED_Z_MATRIX );
#if FEATURE_MILLING_MODE
    g_nActiveWorkPart = (char)readWord24C256( I2C_ADDRESS_EXTERNAL_EEPROM, EEPROM_OFFSET_ACTIVE_WORK_PART_Z_MATRIX );
#endif // FEATURE_MILLING_MODE

    if (Printer::ZMode == Z_VALUE_MODE_SURFACE)
    {
        if( g_ZCompensationMatrix[0][0] != EEPROM_FORMAT )
        {
            // we load the z compensation matrix
            prepareZCompensation();
        }
    }

#if FEATURE_RGB_LIGHT_EFFECTS
    setRGBLEDs( 0, 0, 0 );

    switch( RGBLightMode )
    {
        case RGB_MODE_OFF:
        {
            setRGBTargetColors( 0, 0, 0 );
            break;
        }
        case RGB_MODE_WHITE:
        {
            setRGBTargetColors( 255, 255, 255 );
            break;
        }
        case RGB_MODE_AUTOMATIC:
        {
            setRGBTargetColors( g_uRGBIdleR, g_uRGBIdleG, g_uRGBIdleB );
            break;
        }
        case RGB_MODE_MANUAL:
        {
            setRGBTargetColors( g_uRGBManualR, g_uRGBManualG, g_uRGBManualB );
            break;
        }
    }
#endif // FEATURE_RGB_LIGHT_EFFECTS

#if FEATURE_UNLOCK_MOVEMENT
    g_unlock_movement = 0;
#endif //FEATURE_UNLOCK_MOVEMENT

#if FEATURE_WATCHDOG
    if( Printer::debugInfo() )
    {
        Com::printFLN(Com::tStartWatchdog);
    }
    HAL::startWatchdog();
#endif // FEATURE_WATCHDOG

} // setup()


void Printer::defaultLoopActions()
{
    Commands::checkForPeriodicalActions();  //check heater every n milliseconds

    UI_MEDIUM; // do check encoder
    millis_t curtime = HAL::timeInMilliseconds();

    if( PrintLine::hasLines() )
    {
        previousMillisCmd = curtime;
    }
    else
    {
        curtime -= previousMillisCmd;
        if( maxInactiveTime!=0 && curtime > maxInactiveTime ) Printer::kill(false);
        else Printer::setAllKilled(false); // prevent repeated kills
        if( stepperInactiveTime!=0 && curtime > stepperInactiveTime )
        {
            Printer::kill(true);
        }
    }

#if defined(SDCARDDETECT) && SDCARDDETECT>-1 && defined(SDSUPPORT) && SDSUPPORT
    sd.automount();
#endif // defined(SDCARDDETECT) && SDCARDDETECT>-1 && defined(SDSUPPORT) && SDSUPPORT

    DEBUG_MEMORY;

} // defaultLoopActions


#if FEATURE_MEMORY_POSITION
void Printer::MemoryPosition()
{
    updateCurrentPosition(false);
    lastCalculatedPosition(memoryX,memoryY,memoryZ);
    memoryE = queuePositionLastSteps[E_AXIS]*axisStepsPerMM[E_AXIS];

} // MemoryPosition


void Printer::GoToMemoryPosition(bool x,bool y,bool z,bool e,float feed)
{
    bool all = !(x || y || z);
    float oldFeedrate = feedrate;


    moveToReal((all || x ? memoryX : IGNORE_COORDINATE)
               ,(all || y ? memoryY : IGNORE_COORDINATE)
               ,(all || z ? memoryZ : IGNORE_COORDINATE)
               ,(e ? memoryE:IGNORE_COORDINATE),
               feed);
    feedrate = oldFeedrate;

} // GoToMemoryPosition
#endif // FEATURE_MEMORY_POSITION


void Printer::homeXAxis()
{
    long steps;


    if ((MIN_HARDWARE_ENDSTOP_X && X_MIN_PIN > -1 && X_HOME_DIR==-1) || (MAX_HARDWARE_ENDSTOP_X && X_MAX_PIN > -1 && X_HOME_DIR==1))
    {
        int32_t offX = 0;

#if NUM_EXTRUDER>1
        // Reposition extruder that way, that all extruders can be selected at home pos.
        for(uint8_t i=0; i<NUM_EXTRUDER; i++)
#if X_HOME_DIR < 0
            offX = RMath::max(offX,extruder[i].xOffset);
#else
            offX = RMath::min(offX,extruder[i].xOffset);
#endif // X_HOME_DIR < 0
#endif // NUM_EXTRUDER>1

#if FEATURE_MILLING_MODE
        if( Printer::operatingMode == OPERATING_MODE_MILL )
        {
            // in operating mode mill, there is no extruder offset
            offX = 0;
        }
#endif // FEATURE_MILLING_MODE

        UI_STATUS_UPD(UI_TEXT_HOME_X);
        uid.lock();

        steps = (Printer::maxSteps[X_AXIS]-Printer::minSteps[X_AXIS]) * X_HOME_DIR;
        queuePositionLastSteps[X_AXIS] = -steps;
        setHoming(true);
        PrintLine::moveRelativeDistanceInSteps(2*steps,0,0,0,homingFeedrate[X_AXIS],true,true);
        setHoming(false);
        queuePositionLastSteps[X_AXIS] = (X_HOME_DIR == -1) ? minSteps[X_AXIS]- offX : maxSteps[X_AXIS] + offX;
        PrintLine::moveRelativeDistanceInSteps(axisStepsPerMM[X_AXIS] * -ENDSTOP_X_BACK_MOVE * X_HOME_DIR,0,0,0,homingFeedrate[X_AXIS] / ENDSTOP_X_RETEST_REDUCTION_FACTOR,true,false);
        setHoming(true);
        PrintLine::moveRelativeDistanceInSteps(axisStepsPerMM[X_AXIS] * 2 * ENDSTOP_X_BACK_MOVE * X_HOME_DIR,0,0,0,homingFeedrate[X_AXIS] / ENDSTOP_X_RETEST_REDUCTION_FACTOR,true,true);
        setHoming(false);

#if defined(ENDSTOP_X_BACK_ON_HOME)
        if(ENDSTOP_X_BACK_ON_HOME > 0)
            PrintLine::moveRelativeDistanceInSteps(axisStepsPerMM[X_AXIS] * -ENDSTOP_X_BACK_ON_HOME * X_HOME_DIR,0,0,0,homingFeedrate[X_AXIS],true,false);
#endif // defined(ENDSTOP_X_BACK_ON_HOME)

        queuePositionLastSteps[X_AXIS]    = (X_HOME_DIR == -1) ? minSteps[X_AXIS]-offX : maxSteps[X_AXIS]+offX;
        queuePositionCurrentSteps[X_AXIS] = queuePositionLastSteps[X_AXIS];

#if FEATURE_EXTENDED_BUTTONS || FEATURE_PAUSE_PRINTING
        directPositionTargetSteps[X_AXIS]  = 0;
        directPositionCurrentSteps[X_AXIS] = 0;
        directPositionLastSteps[X_AXIS]    = 0;
#endif // FEATURE_EXTENDED_BUTTONS || FEATURE_PAUSE_PRINTING

#if NUM_EXTRUDER>1
        if( offX )
        {
            PrintLine::moveRelativeDistanceInSteps((Extruder::current->xOffset-offX) * X_HOME_DIR,0,0,0,homingFeedrate[X_AXIS],true,false);
        }
#endif // NUM_EXTRUDER>1

        // show that we are active
        previousMillisCmd = HAL::timeInMilliseconds();
    }

} // homeXAxis


void Printer::homeYAxis()
{
    long steps;


    if ((MIN_HARDWARE_ENDSTOP_Y && Y_MIN_PIN > -1 && Y_HOME_DIR==-1) || (MAX_HARDWARE_ENDSTOP_Y && Y_MAX_PIN > -1 && Y_HOME_DIR==1))
    {
        int32_t offY = 0;

#if NUM_EXTRUDER>1
        // Reposition extruder that way, that all extruders can be selected at home pos.
        for(uint8_t i=0; i<NUM_EXTRUDER; i++)
#if Y_HOME_DIR<0
            offY = RMath::max(offY,extruder[i].yOffset);
#else
            offY = RMath::min(offY,extruder[i].yOffset);
#endif // Y_HOME_DIR<0
#endif // NUM_EXTRUDER>1

#if FEATURE_MILLING_MODE
        if( Printer::operatingMode == OPERATING_MODE_MILL )
        {
            // in operating mode mill, there is no extruder offset
            offY = 0;
        }
#endif // FEATURE_MILLING_MODE

        UI_STATUS_UPD(UI_TEXT_HOME_Y);
        uid.lock();

        steps = (maxSteps[Y_AXIS]-Printer::minSteps[Y_AXIS]) * Y_HOME_DIR;
        queuePositionLastSteps[Y_AXIS] = -steps;
        setHoming(true);
        PrintLine::moveRelativeDistanceInSteps(0,2*steps,0,0,homingFeedrate[Y_AXIS],true,true);
        setHoming(false);
        queuePositionLastSteps[Y_AXIS] = (Y_HOME_DIR == -1) ? minSteps[Y_AXIS]-offY : maxSteps[Y_AXIS]+offY;
        PrintLine::moveRelativeDistanceInSteps(0,axisStepsPerMM[Y_AXIS]*-ENDSTOP_Y_BACK_MOVE * Y_HOME_DIR,0,0,homingFeedrate[Y_AXIS]/ENDSTOP_X_RETEST_REDUCTION_FACTOR,true,false);
        setHoming(true);
        PrintLine::moveRelativeDistanceInSteps(0,axisStepsPerMM[Y_AXIS]*2*ENDSTOP_Y_BACK_MOVE * Y_HOME_DIR,0,0,homingFeedrate[Y_AXIS]/ENDSTOP_X_RETEST_REDUCTION_FACTOR,true,true);
        setHoming(false);

#if defined(ENDSTOP_Y_BACK_ON_HOME)
        if(ENDSTOP_Y_BACK_ON_HOME > 0)
            PrintLine::moveRelativeDistanceInSteps(0,axisStepsPerMM[Y_AXIS]*-ENDSTOP_Y_BACK_ON_HOME * Y_HOME_DIR,0,0,homingFeedrate[Y_AXIS],true,false);
#endif // defined(ENDSTOP_Y_BACK_ON_HOME)

        queuePositionLastSteps[Y_AXIS]    = (Y_HOME_DIR == -1) ? minSteps[Y_AXIS]-offY : maxSteps[Y_AXIS]+offY;
        queuePositionCurrentSteps[Y_AXIS] = queuePositionLastSteps[Y_AXIS];

#if FEATURE_EXTENDED_BUTTONS || FEATURE_PAUSE_PRINTING
        directPositionTargetSteps[Y_AXIS]  = 0;
        directPositionCurrentSteps[Y_AXIS] = 0;
        directPositionLastSteps[Y_AXIS]    = 0;
#endif // FEATURE_EXTENDED_BUTTONS || FEATURE_PAUSE_PRINTING

#if NUM_EXTRUDER>1
        if( offY )
        {
            PrintLine::moveRelativeDistanceInSteps(0,(Extruder::current->yOffset-offY) * Y_HOME_DIR,0,0,homingFeedrate[Y_AXIS],true,false);
        }
#endif // NUM_EXTRUDER>1

        // show that we are active
        previousMillisCmd = HAL::timeInMilliseconds();
    }

} // homeYAxis


void Printer::homeZAxis()
{
    long    steps;
    char    nProcess = 0;
    char    nHomeDir;


#if FEATURE_MILLING_MODE

    nProcess = 1;
    if( Printer::operatingMode == OPERATING_MODE_PRINT )
    {
        // in operating mode "print" we use the z min endstop
        nHomeDir = -1;
    }
    else
    {
        // in operating mode "mill" we use the z max endstop
        nHomeDir = 1;
    }

#else

    if ((MIN_HARDWARE_ENDSTOP_Z && Z_MIN_PIN > -1 && Z_HOME_DIR==-1) || (MAX_HARDWARE_ENDSTOP_Z && Z_MAX_PIN > -1 && Z_HOME_DIR==1))
    {
        nProcess = 1;
        nHomeDir = Z_HOME_DIR;
    }

#endif // FEATURE_MILLING_MODE

    // if we have circuit-type Z endstops and we don't know at which endstop we currently are, first move down a bit
#if FEATURE_CONFIGURABLE_Z_ENDSTOPS
    if( Printer::ZEndstopUnknown ) {
        PrintLine::moveRelativeDistanceInSteps(0,0,axisStepsPerMM[Z_AXIS] * -1 * ENDSTOP_Z_BACK_MOVE * nHomeDir,0,homingFeedrate[Z_AXIS]/ENDSTOP_Z_RETEST_REDUCTION_FACTOR,true,false);
    }
#endif

    if( nProcess )
    {
        UI_STATUS_UPD( UI_TEXT_HOME_Z );
        uid.lock();

#if FEATURE_FIND_Z_ORIGIN
        g_nZOriginPosition[X_AXIS] = 0;
        g_nZOriginPosition[Y_AXIS] = 0;
        g_nZOriginPosition[Z_AXIS] = 0;
#endif // FEATURE_FIND_Z_ORIGIN

#if FEATURE_EXTENDED_BUTTONS || FEATURE_PAUSE_PRINTING
        directPositionTargetSteps[Z_AXIS]  = 0;
        directPositionCurrentSteps[Z_AXIS] = 0;
        directPositionLastSteps[Z_AXIS]    = 0;
#endif // FEATURE_EXTENDED_BUTTONS || FEATURE_PAUSE_PRINTING

        setHomed( false , -1 , -1 , false);

#if FEATURE_HEAT_BED_Z_COMPENSATION || FEATURE_WORK_PART_Z_COMPENSATION
        g_nZScanZPosition = 0;
        queueTask( TASK_DISABLE_Z_COMPENSATION );
#endif // FEATURE_HEAT_BED_Z_COMPENSATION || FEATURE_WORK_PART_Z_COMPENSATION

        steps = (maxSteps[Z_AXIS] - minSteps[Z_AXIS]) * nHomeDir;
        queuePositionLastSteps[Z_AXIS] = -steps;
        setHoming(true);
        PrintLine::moveRelativeDistanceInSteps(0,0,2*steps,0,homingFeedrate[Z_AXIS],true,true);
        setHoming(false);
        queuePositionLastSteps[Z_AXIS] = (nHomeDir == -1) ? minSteps[Z_AXIS] : maxSteps[Z_AXIS];
        PrintLine::moveRelativeDistanceInSteps(0,0,axisStepsPerMM[Z_AXIS] * -1 * ENDSTOP_Z_BACK_MOVE * nHomeDir,0,homingFeedrate[Z_AXIS]/ENDSTOP_Z_RETEST_REDUCTION_FACTOR,true,false);
        setHoming(true);
        PrintLine::moveRelativeDistanceInSteps(0,0,axisStepsPerMM[Z_AXIS] * 2 * ENDSTOP_Z_BACK_MOVE * nHomeDir,0,homingFeedrate[Z_AXIS]/ENDSTOP_Z_RETEST_REDUCTION_FACTOR,true,true);
        setHoming(false);

#if FEATURE_MILLING_MODE
        // when the milling mode is active and we are in operating mode "mill", we use the z max endstop and we free the z-max endstop after it has been hit
        if( Printer::operatingMode == OPERATING_MODE_MILL )
        {
            PrintLine::moveRelativeDistanceInSteps(0,0,LEAVE_Z_MAX_ENDSTOP_AFTER_HOME,0,homingFeedrate[Z_AXIS],true,false);
        }
#else

#if defined(ENDSTOP_Z_BACK_ON_HOME)
        if(ENDSTOP_Z_BACK_ON_HOME > 0)
            PrintLine::moveRelativeDistanceInSteps(0,0,axisStepsPerMM[Z_AXIS]*-ENDSTOP_Z_BACK_ON_HOME * nHomeDir,0,homingFeedrate[Z_AXIS],true,false);
#endif // defined(ENDSTOP_Z_BACK_ON_HOME)
#endif // FEATURE_MILLING_MODE

        queuePositionLastSteps[Z_AXIS]    = (nHomeDir == -1) ? minSteps[Z_AXIS] : maxSteps[Z_AXIS];
        queuePositionCurrentSteps[Z_AXIS] = queuePositionLastSteps[Z_AXIS];

#if FEATURE_Z_MIN_OVERRIDE_VIA_GCODE
        currentZSteps                     = queuePositionLastSteps[Z_AXIS];
#endif // FEATURE_Z_MIN_OVERRIDE_VIA_GCODE

        // show that we are active
        previousMillisCmd = HAL::timeInMilliseconds();
    }

} // homeZAxis


void Printer::homeAxis(bool xaxis,bool yaxis,bool zaxis) // home non-delta printer
{
    float   startX,startY,startZ;
    char    homingOrder;
    char    unlock = !uid.locked;
    
    //Bei beliebiger user interaktion oder Homing soll G1 etc. erlaubt werden. Dann ist der Drucker nicht abgestürzt, sondern bedient worden.
#if FEATURE_UNLOCK_MOVEMENT
    g_unlock_movement = 1;
#endif //FEATURE_UNLOCK_MOVEMENT
    lastCalculatedPosition(startX,startY,startZ);

#if FEATURE_MILLING_MODE
    if( operatingMode == OPERATING_MODE_PRINT )
    {
        homingOrder = HOMING_ORDER_PRINT;
    }
    else
    {
        homingOrder = HOMING_ORDER_MILL;
    }
#else
    homingOrder = HOMING_ORDER;
#endif // FEATURE_MILLING_MODE
    if(operatingMode == OPERATING_MODE_PRINT){
      if( (!yaxis && zaxis) || ( homingOrder == HOME_ORDER_XZY && homingOrder == HOME_ORDER_ZXY && homingOrder == HOME_ORDER_ZYX ) )
      {
       // do not allow homing Z-Only within menu, when the Extruder is configured < 0 and over bed.
       if( !Printer::isZHomeSafe() && !g_nHeatBedScanStatus ) //beim HBS wird z gehomed wenn was schief geht: g_nHeatBedScanStatus => (45,105,139,105) das düfte nicht passieren, wenn man n glas höher endschalter auf der Heizplatte hat. Hier Y homen wäre saublöd, drum mach ichs nicht. -> Zukunft: Man sollte das Homing aus dem HBS streichen!
       {
          homeYAxis(); //bevor die düse gegen das bett knallen könnte, weil positive z-matix oder tipdown-extruder sollte erst y genullt werden: kann das im printermode schädlich sein?
          //wenn Z genullt wird, sollte auch Y genullt werden dürfen.
       }
      }
    }

    switch( homingOrder )
    {
        case HOME_ORDER_XYZ:
        {
            if(xaxis) homeXAxis();
            if(yaxis) homeYAxis();
            if(zaxis) homeZAxis();
            break;
        }
        case HOME_ORDER_XZY:
        {
            if(xaxis) homeXAxis();
            if(zaxis) homeZAxis();
            if(yaxis) homeYAxis();
            break;
        }
        case HOME_ORDER_YXZ:
        {
            if(yaxis) homeYAxis();
            if(xaxis) homeXAxis();
            if(zaxis) homeZAxis();
            break;
        }
        case HOME_ORDER_YZX:
        {
            if(yaxis) homeYAxis();
            if(zaxis) homeZAxis();
            if(xaxis) homeXAxis();
            break;
        }
        case HOME_ORDER_ZXY:
        {
            if(zaxis) homeZAxis();
            if(xaxis) homeXAxis();
            if(yaxis) homeYAxis();
            break;
        }
        case HOME_ORDER_ZYX:
        {
            if(zaxis) homeZAxis();
            if(yaxis) homeYAxis();
            if(xaxis) homeXAxis();
            break;
        }
    }

    if(xaxis)
    {
        if(X_HOME_DIR<0) startX = Printer::minMM[X_AXIS];
        else startX = Printer::minMM[X_AXIS]+Printer::lengthMM[X_AXIS];
    }
    if(yaxis)
    {
        if(Y_HOME_DIR<0) startY = Printer::minMM[Y_AXIS];
        else startY = Printer::minMM[Y_AXIS]+Printer::lengthMM[Y_AXIS];
    }
    if(zaxis)
    {
#if FEATURE_MILLING_MODE
        if( Printer::operatingMode == OPERATING_MODE_PRINT )
        {
            startZ = Printer::minMM[Z_AXIS];
        }
        else
        {
            startZ = Printer::minMM[Z_AXIS]+Printer::lengthMM[Z_AXIS];
        }
#else

        if(Z_HOME_DIR<0) startZ = Printer::minMM[Z_AXIS];
        else startZ = Printer::minMM[Z_AXIS]+Printer::lengthMM[Z_AXIS];

#endif // FEATURE_MILLING_MODE

        setZOriginSet(false);

#if FEATURE_CONFIGURABLE_Z_ENDSTOPS
        ZEndstopUnknown = false;
#endif // FEATURE_CONFIGURABLE_Z_ENDSTOPS
    }
    updateCurrentPosition(true);
    
    
    moveToReal(startX,startY,startZ,IGNORE_COORDINATE,homingFeedrate[X_AXIS]);

    setHomed(true, (xaxis?true:-1), (yaxis?true:-1), (zaxis?true:-1) );
    
#if FEATURE_ZERO_DIGITS
    short   nTempPressure = 0;
    if(Printer::g_pressure_offset_active && xaxis && yaxis && zaxis){ //only adjust pressure if you do a full homing.
        Printer::g_pressure_offset = 0; //prevent to messure already adjusted offset -> without = 0 this would only iterate some bad values.
        if( !readAveragePressure( &nTempPressure ) ){
            if(-5000 < nTempPressure && nTempPressure < 5000){
                Com::printFLN( PSTR( "DigitOffset = " ), nTempPressure );
                Printer::g_pressure_offset = nTempPressure;
            }else{
                //those high values shouldnt happen! fix your machine... DONT ZEROSCALE DIGITS
                Com::printFLN( PSTR( "DigitOffset failed " ), nTempPressure );
            }
        } else{
            Com::printFLN( PSTR( "DigitOffset failed reading " ) );
            g_abortZScan = 0;
        }
    }
#endif // FEATURE_ZERO_DIGITS

    if( unlock )
    {
        uid.unlock();
    }
    g_uStartOfIdle = HAL::timeInMilliseconds();
    Commands::printCurrentPosition();

} // homeAxis


bool Printer::allowQueueMove( void )
{
    if( g_pauseStatus == PAUSE_STATUS_PAUSED ) return false;

    if( !( (PAUSE_STATUS_GOTO_PAUSE1 <= g_pauseStatus && g_pauseStatus <= PAUSE_STATUS_HEATING) || g_pauseStatus == PAUSE_STATUS_NONE) 
        && !PrintLine::cur )
    {
        // do not allow to process new moves from the queue while the printing is paused
        return false;
    }

    if( !PrintLine::hasLines() )
    {
        // do not allow to process moves from the queue in case there is no queue
        return false;
    }

    // we are not paused and there are moves in our queue - we should process them
    return true;

} // allowQueueMove


#if FEATURE_EXTENDED_BUTTONS || FEATURE_PAUSE_PRINTING
bool Printer::allowDirectMove( void )
{
    if( PrintLine::direct.stepsRemaining )
    {
        // the currently known direct movements must be processed as move
        return true;
    }

    // the currently known direct movements must be processed as single steps
    return false;

} // allowDirectMove


bool Printer::allowDirectSteps( void )
{
    if( PrintLine::direct.stepsRemaining )
    {
        // the currently known direct movements must be processed as move
        return false;
    }

    if( endZCompensationStep )
    {
        // while zcompensation is working it has higher priority
        return false;
    }

    // the currently known direct movements must be processed as single steps
    return true;

} // allowDirectSteps


bool Printer::processAsDirectSteps( void )
{
    if( PrintLine::linesCount || waitMove )
    {
        // we are printing at the moment, thus all direct movements must be processed as single steps
        return true;
    }

    // we are not printing at the moment, thus all direct movements must be processed as move
    return false;

} // processAsDirectSteps


void Printer::resetDirectPosition( void )
{
    unsigned char   axis;


    // we may have to update our x/y/z queue positions - there is no need/sense to update the extruder queue position
    for( axis=0; axis<3; axis++ )
    {
        if( directPositionCurrentSteps[axis] )
        {
            queuePositionCurrentSteps[axis] += directPositionCurrentSteps[axis];
            queuePositionLastSteps[axis]    += directPositionCurrentSteps[axis];
            queuePositionTargetSteps[axis]  += directPositionCurrentSteps[axis];

            queuePositionCommandMM[axis]    = 
            queuePositionLastMM[axis]       = (float)(queuePositionLastSteps[axis])*invAxisStepsPerMM[axis];
        }
    }

    directPositionTargetSteps[X_AXIS]  = 
    directPositionTargetSteps[Y_AXIS]  = 
    directPositionTargetSteps[Z_AXIS]  = 
    directPositionTargetSteps[E_AXIS]  = 
    directPositionCurrentSteps[X_AXIS] = 
    directPositionCurrentSteps[Y_AXIS] = 
    directPositionCurrentSteps[Z_AXIS] = 
    directPositionCurrentSteps[E_AXIS] = 0;
    return;

} // resetDirectPosition
#endif // FEATURE_EXTENDED_BUTTONS || FEATURE_PAUSE_PRINTING


#if FEATURE_HEAT_BED_Z_COMPENSATION || FEATURE_WORK_PART_Z_COMPENSATION
void Printer::performZCompensation( void )
{
    // the order of the following checks shall not be changed
#if FEATURE_HEAT_BED_Z_COMPENSATION && FEATURE_WORK_PART_Z_COMPENSATION
    if( !doHeatBedZCompensation && !doWorkPartZCompensation )
    {
        return;
    }
#elif FEATURE_HEAT_BED_Z_COMPENSATION
    if( !doHeatBedZCompensation )
    {
        return;
    }
#elif FEATURE_WORK_PART_Z_COMPENSATION
    if( !doWorkPartZCompensation )
    {
        return;
    }
#endif // FEATURE_HEAT_BED_Z_COMPENSATION && FEATURE_WORK_PART_Z_COMPENSATION

    if( blockAll )
    {
        // do not perform any compensation in case the moving is blocked
        return;
    }

    if( endZCompensationStep ) //zuerst beenden, bevor irgendwas neues an Z manipuliert werden darf. 20.07.2017 Nibbels
    {
#if STEPPER_HIGH_DELAY>0
        HAL::delayMicroseconds( STEPPER_HIGH_DELAY );
#endif // STEPPER_HIGH_DELAY>0

        endZStep();
        compensatedPositionCurrentStepsZ += endZCompensationStep;
        endZCompensationStep = 0;

        //Insert little wait if next Z step might follow now.
        if( isDirectOrQueueOrCompZMove() )
        {
#if STEPPER_HIGH_DELAY+DOUBLE_STEP_DELAY>0
            HAL::delayMicroseconds(STEPPER_HIGH_DELAY+DOUBLE_STEP_DELAY);
#endif // STEPPER_HIGH_DELAY+DOUBLE_STEP_DELAY>0
            return;
        }
        else if( compensatedPositionCurrentStepsZ == compensatedPositionTargetStepsZ /* && not isDirectOrQueueOrCompZMove() */ ) stepperDirection[Z_AXIS] = 0; //-> Ich glaube, das brauchen wir hier nicht, wenn der DirectMove sauber abschließt. Hier wird nur reingeschummelt wenn ein DirectMove läuft.

        return;
    }

    if( PrintLine::cur )
    {
        if( PrintLine::cur->isZMove() )
        {
            // do not peform any compensation while there is a "real" move into z-direction
            if( PrintLine::cur->stepsRemaining ) return;
            else PrintLine::cur->setZMoveFinished();
        }
    }

    if( PrintLine::direct.isZMove() )
    {
        // do not peform any compensation while there is a direct-move into z-direction
        if( PrintLine::direct.stepsRemaining ) return;
        else PrintLine::direct.setZMoveFinished();
    }

    if( compensatedPositionCurrentStepsZ < compensatedPositionTargetStepsZ )
    {
        // we must move the heat bed do the bottom
        if( stepperDirection[Z_AXIS] >= 0 )
        {
            // here we shall move the z-axis only in case performQueueMove() is not moving into the other direction at the moment
            if( stepperDirection[Z_AXIS] == 0 )
            {
                // set the direction only in case it is not set already
                prepareBedDown();
                stepperDirection[Z_AXIS] = 1;
                //return;
            }
            startZStep( stepperDirection[Z_AXIS] );
            endZCompensationStep = stepperDirection[Z_AXIS];
        }

        return;
    }

    if( compensatedPositionCurrentStepsZ > compensatedPositionTargetStepsZ )
    {
        // we must move the heat bed to the top
        if( stepperDirection[Z_AXIS] <= 0 )
        {
            // here we shall move the z-axis only in case performQueueMove() is not moving into the other direction at the moment
            if( stepperDirection[Z_AXIS] == 0 )
            {
                // set the direction only in case it is not set already
                prepareBedUp();
                stepperDirection[Z_AXIS] = -1;
                //return;
            }
            startZStep( stepperDirection[Z_AXIS] );
            endZCompensationStep = stepperDirection[Z_AXIS];
        }

        return;
    }

} // performZCompensation


void Printer::resetCompensatedPosition( void )
{
    if( compensatedPositionCurrentStepsZ )
    {
        queuePositionCurrentSteps[Z_AXIS] += compensatedPositionCurrentStepsZ;
        queuePositionLastSteps[Z_AXIS]    += compensatedPositionCurrentStepsZ;
        queuePositionTargetSteps[Z_AXIS]  += compensatedPositionCurrentStepsZ;

#if FEATURE_Z_MIN_OVERRIDE_VIA_GCODE
        currentZSteps                     += compensatedPositionCurrentStepsZ;
#endif // FEATURE_Z_MIN_OVERRIDE_VIA_GCODE

        queuePositionCommandMM[Z_AXIS]  = 
        queuePositionLastMM[Z_AXIS]     = (float)(queuePositionLastSteps[Z_AXIS])*invAxisStepsPerMM[Z_AXIS];
    }

    compensatedPositionTargetStepsZ  = 0;
    compensatedPositionCurrentStepsZ = 0;
    endZCompensationStep             = 0;

} // resetCompensatedPosition

#endif // FEATURE_HEAT_BED_Z_COMPENSATION || FEATURE_WORK_PART_Z_COMPENSATION
