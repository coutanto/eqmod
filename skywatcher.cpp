/* Copyright 2012 Geehalel (geehalel AT gmail DOT com) */
/* This file is part of the Skywatcher Protocol INDI driver.

    The Skywatcher Protocol INDI driver is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    The Skywatcher Protocol INDI driver is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with the Skywatcher Protocol INDI driver.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "skywatcher.h"

#include "eqmodbase.h"

#include <indicom.h>

#include <termios.h>
#include <cmath>
#include <cstring>

Skywatcher::Skywatcher(EQMod *t)
{
    debug         = false;
    debugnextread = false;
    simulation    = false;
    telescope     = t;
    reconnect     = false;
}

Skywatcher::~Skywatcher(void)
{
    Disconnect();
}

void Skywatcher::setDebug(bool enable)
{
    debug = enable;
}
bool Skywatcher::isDebug()
{
    return debug;
}

void Skywatcher::setPortFD(int value)
{
    PortFD = value;
}

void Skywatcher::setSimulation(bool enable)
{
    simulation = enable;
}
bool Skywatcher::isSimulation()
{
    return simulation;
}

const char *Skywatcher::getDeviceName()
{
    return telescope->getDeviceName();
}

/* API */

bool Skywatcher::Handshake()
{
    if (isSimulation())
    {
        telescope->simulator->Connect();
    }

    uint32_t tmpMCVersion = 0;

    dispatch_command(InquireMotorBoardVersion, Axis1, nullptr);
    //read_eqmod();
    tmpMCVersion = Revu24str2long(response + 1);
    MCVersion    = ((tmpMCVersion & 0xFF) << 16) | ((tmpMCVersion & 0xFF00)) | ((tmpMCVersion & 0xFF0000) >> 16);
    MountCode    = MCVersion & 0xFF;
    /* Check supported mounts here */
    if ((MountCode == 0x80) || (MountCode == 0x81) /*|| (MountCode == 0x82)*/ || (MountCode == 0x90))
    {
        throw EQModError(EQModError::ErrDisconnect,
                         "Mount not supported: mount code 0x%x (0x80=GT, 0x81=MF, 0x82=114GT, 0x90=DOB)", MountCode);
        //return false;
    }

    return true;
}

bool Skywatcher::Disconnect()
{
    if (PortFD < 0)
        return true;

    try
    {
        StopMotor(Axis1);
        StopMotor(Axis2);
    }
    catch (EQModError)
    {
        // Ignore error
    }

    return true;
}

uint32_t Skywatcher::GetRAEncoder()
{
    // Axis Position
    dispatch_command(GetAxisPosition, Axis1, nullptr);

    uint32_t steps = Revu24str2long(response + 1);
    if (steps & 0x80000000)
        DEBUGF(telescope->DBG_SCOPE_STATUS, "%s() = Ignoring invalid response %s", __FUNCTION__, response);
    else
        RAStep = steps;

    gettimeofday(&lastreadmotorposition[Axis1], nullptr);
    if (RAStep != lastRAStep)
    {
        DEBUGF(telescope->DBG_SCOPE_STATUS, "%s() = %ld", __FUNCTION__, static_cast<long>(RAStep));
        lastRAStep = RAStep;
    }
    return RAStep;
}

uint32_t Skywatcher::GetDEEncoder()
{
    // Axis Position
    dispatch_command(GetAxisPosition, Axis2, nullptr);

    uint32_t steps = Revu24str2long(response + 1);
    if (steps & 0x80000000)
        DEBUGF(telescope->DBG_SCOPE_STATUS, "%s() = Ignoring invalid response %s", __FUNCTION__, response);
    else
        DEStep = steps;
    gettimeofday(&lastreadmotorposition[Axis2], nullptr);
    if (DEStep != lastDEStep)
    {
        DEBUGF(telescope->DBG_SCOPE_STATUS, "%s() = %ld", __FUNCTION__, static_cast<long>(DEStep));
        lastDEStep = DEStep;
    }
    return DEStep;
}

uint32_t Skywatcher::GetRAEncoderZero()
{
    LOGF_DEBUG("%s() = %ld", __FUNCTION__, static_cast<long>(RAStepInit));
    return RAStepInit;
}

uint32_t Skywatcher::GetRAEncoderTotal()
{
    LOGF_DEBUG("%s() = %ld", __FUNCTION__, static_cast<long>(RASteps360));
    return RASteps360;
}

uint32_t Skywatcher::GetRAEncoderHome()
{
    LOGF_DEBUG("%s() = %ld", __FUNCTION__, static_cast<long>(RAStepHome));
    return RAStepHome;
}

uint32_t Skywatcher::GetDEEncoderZero()
{
    LOGF_DEBUG("%s() = %ld", __FUNCTION__, static_cast<long>(DEStepInit));
    return DEStepInit;
}

uint32_t Skywatcher::GetDEEncoderTotal()
{
    LOGF_DEBUG("%s() = %ld", __FUNCTION__, static_cast<long>(DESteps360));
    return DESteps360;
}

uint32_t Skywatcher::GetDEEncoderHome()
{
    LOGF_DEBUG("%s() = %ld", __FUNCTION__, static_cast<long>(DEStepHome));
    return DEStepHome;
}

uint32_t Skywatcher::GetRAPeriod()
{
    if (RAPeriod != lastRAPeriod)
    {
        DEBUGF(telescope->DBG_SCOPE_STATUS, "%s() = %ld", __FUNCTION__, static_cast<long>(RAPeriod));
        lastRAPeriod = RAPeriod;
    }
    return RAPeriod;
}

uint32_t Skywatcher::GetDEPeriod()
{
    if (DEPeriod != lastDEPeriod)
    {
        DEBUGF(telescope->DBG_SCOPE_STATUS, "%s() = %ld", __FUNCTION__, static_cast<long>(DEPeriod));
        lastDEPeriod = DEPeriod;
    }
    return DEPeriod;
}

uint32_t Skywatcher::GetlastreadRAIndexer()
{
    if (MountCode != 0x04 && MountCode != 0x05 && MountCode != 0x20 && MountCode != 0x25)
        throw EQModError(EQModError::ErrInvalidCmd, "Incorrect mount type");
    DEBUGF(telescope->DBG_SCOPE_STATUS, "%s() = %ld", __FUNCTION__, static_cast<long>(lastreadIndexer[Axis1]));
    return lastreadIndexer[Axis1];
}

uint32_t Skywatcher::GetlastreadDEIndexer()
{
    if (MountCode != 0x04 && MountCode != 0x05 && MountCode != 0x20 && MountCode != 0x25)
        throw EQModError(EQModError::ErrInvalidCmd, "Incorrect mount type");
    DEBUGF(telescope->DBG_SCOPE_STATUS, "%s() = %ld", __FUNCTION__, static_cast<long>(lastreadIndexer[Axis2]));
    return lastreadIndexer[Axis2];
}

// deprecated
void Skywatcher::GetRAMotorStatus(ILightVectorProperty *motorLP)
{
    ReadMotorStatus(Axis1);
    if (!RAInitialized)
    {
        IUFindLight(motorLP, "RAInitialized")->s = IPS_ALERT;
        IUFindLight(motorLP, "RARunning")->s     = IPS_IDLE;
        IUFindLight(motorLP, "RAGoto")->s        = IPS_IDLE;
        IUFindLight(motorLP, "RAForward")->s     = IPS_IDLE;
        IUFindLight(motorLP, "RAHighspeed")->s   = IPS_IDLE;
    }
    else
    {
        IUFindLight(motorLP, "RAInitialized")->s = IPS_OK;
        IUFindLight(motorLP, "RARunning")->s     = (RARunning ? IPS_OK : IPS_BUSY);
        IUFindLight(motorLP, "RAGoto")->s        = ((RAStatus.slewmode == GOTO) ? IPS_OK : IPS_BUSY);
        IUFindLight(motorLP, "RAForward")->s     = ((RAStatus.direction == FORWARD) ? IPS_OK : IPS_BUSY);
        IUFindLight(motorLP, "RAHighspeed")->s   = ((RAStatus.speedmode == HIGHSPEED) ? IPS_OK : IPS_BUSY);
    }
}

void Skywatcher::GetRAMotorStatus(INDI::PropertyLight motorLP)
{
    ReadMotorStatus(Axis1);
    if (!RAInitialized)
    {
        motorLP.findWidgetByName("RAInitialized")->setState(IPS_ALERT);
        motorLP.findWidgetByName("RARunning")->setState(IPS_IDLE);
        motorLP.findWidgetByName("RAGoto")->setState(IPS_IDLE);
        motorLP.findWidgetByName("RAForward")->setState(IPS_IDLE);
        motorLP.findWidgetByName("RAHighspeed")->setState(IPS_IDLE);
    }
    else
    {
        motorLP.findWidgetByName("RAInitialized")->setState(IPS_OK);
        motorLP.findWidgetByName("RARunning")->setState((RARunning ? IPS_OK : IPS_BUSY));
        motorLP.findWidgetByName("RAGoto")->setState(((RAStatus.slewmode == GOTO) ? IPS_OK : IPS_BUSY));
        motorLP.findWidgetByName("RAForward")->setState(((RAStatus.direction == FORWARD) ? IPS_OK : IPS_BUSY));
        motorLP.findWidgetByName("RAHighspeed")->setState(((RAStatus.speedmode == HIGHSPEED) ? IPS_OK : IPS_BUSY));
    }
}

// deprecated
void Skywatcher::GetDEMotorStatus(ILightVectorProperty *motorLP)
{
    ReadMotorStatus(Axis2);
    if (!DEInitialized)
    {
        IUFindLight(motorLP, "DEInitialized")->s = IPS_ALERT;
        IUFindLight(motorLP, "DERunning")->s     = IPS_IDLE;
        IUFindLight(motorLP, "DEGoto")->s        = IPS_IDLE;
        IUFindLight(motorLP, "DEForward")->s     = IPS_IDLE;
        IUFindLight(motorLP, "DEHighspeed")->s   = IPS_IDLE;
    }
    else
    {
        IUFindLight(motorLP, "DEInitialized")->s = IPS_OK;
        IUFindLight(motorLP, "DERunning")->s     = (DERunning ? IPS_OK : IPS_BUSY);
        IUFindLight(motorLP, "DEGoto")->s        = ((DEStatus.slewmode == GOTO) ? IPS_OK : IPS_BUSY);
        IUFindLight(motorLP, "DEForward")->s     = ((DEStatus.direction == FORWARD) ? IPS_OK : IPS_BUSY);
        IUFindLight(motorLP, "DEHighspeed")->s   = ((DEStatus.speedmode == HIGHSPEED) ? IPS_OK : IPS_BUSY);
    }
}

void Skywatcher::GetDEMotorStatus(INDI::PropertyLight motorLP)
{
    ReadMotorStatus(Axis2);
    if (!DEInitialized)
    {
        motorLP.findWidgetByName("DEInitialized")->setState(IPS_ALERT);
        motorLP.findWidgetByName("DERunning")->setState(IPS_IDLE);
        motorLP.findWidgetByName("DEGoto")->setState(IPS_IDLE);
        motorLP.findWidgetByName("DEForward")->setState(IPS_IDLE);
        motorLP.findWidgetByName("DEHighspeed")->setState(IPS_IDLE);
    }
    else
    {
        motorLP.findWidgetByName("DEInitialized")->setState(IPS_OK);
        motorLP.findWidgetByName("DERunning")->setState((DERunning ? IPS_OK : IPS_BUSY));
        motorLP.findWidgetByName("DEGoto")->setState(((DEStatus.slewmode == GOTO) ? IPS_OK : IPS_BUSY));
        motorLP.findWidgetByName("DEForward")->setState(((DEStatus.direction == FORWARD) ? IPS_OK : IPS_BUSY));
        motorLP.findWidgetByName("DEHighspeed")->setState(((DEStatus.speedmode == HIGHSPEED) ? IPS_OK : IPS_BUSY));
    }
}

void Skywatcher::Init()
{
    wasinitialized = false;
    ReadMotorStatus(Axis1);
    ReadMotorStatus(Axis2);
    
    if (!RAInitialized && !DEInitialized)
    {
        //Read initial stepper values
        dispatch_command(GetAxisPosition, Axis1, nullptr);
        //read_eqmod();
        RAStepInit = Revu24str2long(response + 1);
        
        //LOGF_INFO("modifOC RAStepInit from mount %d %ld \n",RAStepInit,RAStepInit-0x800000);

        //RAStepInit = 8388608; //modifOC
        dispatch_command(GetAxisPosition, Axis2, nullptr);
        //read_eqmod();
        DEStepInit = Revu24str2long(response + 1);
        //LOGF_INFO("modifOC DEStepInit from mount %d %ld \n",DEStepInit,DEStepInit-0x800000);
        //DEStepInit = 8388608; //modifOC
        
        LOGF_DEBUG("%s() : Motors not initialized -- read Init steps RAInit=%ld DEInit = %ld",
                   __FUNCTION__, static_cast<long>(RAStepInit), static_cast<long>(DEStepInit));
        // Energize motors
        LOGF_DEBUG("%s() : Powering motors", __FUNCTION__);
        dispatch_command(Initialize, Axis1, nullptr);
        //read_eqmod();
        dispatch_command(Initialize, Axis2, nullptr);
        //read_eqmod();
#ifdef EQMODE_EXT        
        RAStepHome = RAStepInit + RAHomeInitOffset/24. * RASteps360;
        DEStepHome = DEStepInit + DEHomeInitOffset/360. * DESteps360;
#else
        RAStepHome = RAStepInit;
        DEStepHome = DEStepInit + (DESteps360 / 4);
#endif
    }
    else
    {
        // Mount already initialized by another driver / driver instance
        // use default configuration && leave unchanged encoder values
        wasinitialized = true;
        
#ifdef EQMODE_EXT
        SetMountDependantParameter(MountCode);
        RAStepHome = RAStepInit + RAHomeInitOffset/24.*RASteps360;
        DEStepHome = DEStepInit + DEHomeInitOffset/360.*DESteps360;
#else
        RAStepInit     = 0x800000;
        DEStepInit     = 0x800000;
        RAStepHome     = RAStepInit ;  
        DEStepHome     = DEStepInit + (DESteps360 / 4);
#endif
        LOGF_WARN("%s() : Motors already initialized", __FUNCTION__);
        LOGF_WARN("%s() : Setting default Init steps --  RAInit=%ld DEInit = %ld", __FUNCTION__,
                  static_cast<long>(RAStepInit), static_cast<long>(DEStepInit));
        //LOGF_INFO("modifOC %s() : Motors already initialized", __FUNCTION__);
        //LOGF_INFO("modifOC %s() : Setting default Init steps --  RAInit=%ld DEInit = %ld", __FUNCTION__,
                  static_cast<long>(RAStepInit), static_cast<long>(DEStepInit));
    }
    LOGF_DEBUG("%s() : Setting Home steps RAHome=%ld DEHome = %ld", __FUNCTION__,
               static_cast<long>(RAStepHome), static_cast<long>(DEStepHome));
    //LOGF_INFO("modifOC %s() : Setting Home steps RAHome=%ld DEHome = %ld", __FUNCTION__,
               //static_cast<long>(RAStepHome), static_cast<long>(DEStepHome));

    if (not(reconnect))
    {
        reconnect = true;
        LOGF_WARN("%s() : First Initialization for this driver instance", __FUNCTION__);
        // Initialize unreadable mount feature
        //SetST4RAGuideRate('2');
        //SetST4DEGuideRate('2');
        //LOGF_WARN("%s() : Setting both ST4 guide rates to  0.5x (2)", __FUNCTION__);
    }

    // Problem with buildSkeleton: props are lost between connection/reconnections
    // should reset unreadable mount feature
    SetST4RAGuideRate('2');
    SetST4DEGuideRate('2');
    LOGF_WARN("%s() : Setting both ST4 guide rates to  0.5x (2)", __FUNCTION__);

    if (HasSnapPort1())
    {
        TurnSnapPort1(false);
        LOGF_DEBUG("%s() : Resetting snap port 1", __FUNCTION__);
    }
    if (HasSnapPort2())
    {
        TurnSnapPort2(false);
        LOGF_DEBUG("%s() : Resetting snap port 2", __FUNCTION__);
    }

    //Park status
    if (telescope->InitPark() == false)
    {
        telescope->SetAxis1Park(RAStepHome);
        telescope->SetAxis1ParkDefault(RAStepHome);

        telescope->SetAxis2Park(DEStepHome);
        telescope->SetAxis2ParkDefault(DEStepHome);

        LOGF_WARN("Loading parking data failed. Setting parking axis1: %d axis2: %d", RAStepHome, DEStepHome);

        // JM 2018-11-26: Save current position as parked position
        telescope->saveInitialParkPosition();
    }
    else
    {
        telescope->SetAxis1ParkDefault(RAStepHome);
        telescope->SetAxis2ParkDefault(DEStepHome);
    }

    if (telescope->isParked())
    {
        //TODO get Park position, set corresponding encoder values, mark mount as parked
        //parkSP->sp[0].s==ISS_ON
        LOGF_DEBUG("%s() : Mount was parked", __FUNCTION__);
        //if (wasinitialized) {
        //  LOGF_DEBUG("%s() : leaving encoders unchanged",
        //	     __FUNCTION__);
        //} else {
        char cmdarg[7];
        LOGF_DEBUG("%s() : Mount in Park position -- setting encoders RA=%ld DE = %ld",
                   __FUNCTION__, static_cast<long>(telescope->GetAxis1Park()), static_cast<long>(telescope->GetAxis2Park()));
        cmdarg[6] = '\0';
        long2Revu24str(telescope->GetAxis1Park(), cmdarg);
        dispatch_command(SetAxisPositionCmd, Axis1, cmdarg);
        //read_eqmod();
        cmdarg[6] = '\0';
        long2Revu24str(telescope->GetAxis2Park(), cmdarg);
        dispatch_command(SetAxisPositionCmd, Axis2, cmdarg);
        //read_eqmod();
        //}
    }
    else
    {
        LOGF_DEBUG("%s() : Mount was not parked", __FUNCTION__);
        if (wasinitialized)
        {
            LOGF_DEBUG("%s() : leaving encoders unchanged", __FUNCTION__);
        }
        else
        {
            //mount is supposed to be in the home position (pointing Celestial Pole)
            char cmdarg[7];
            LOGF_DEBUG("%s() : Mount in Home position -- setting encoders RA=%ld DE = %ld",
                       __FUNCTION__, static_cast<long>(RAStepHome), static_cast<long>(DEStepHome));
            cmdarg[6] = '\0';
            long2Revu24str(DEStepHome, cmdarg);
            dispatch_command(SetAxisPositionCmd, Axis2, cmdarg);
            //read_eqmod();
            //LOGF_WARN("%s() : Mount is supposed to point North/South Celestial Pole", __FUNCTION__);
            //TODO mark mount as unparked?
        }
    }
}

// deprecated
void Skywatcher::InquireBoardVersion(ITextVectorProperty *boardTP)
{
    unsigned nprop             = 0;
    char *boardinfo[3];
    const char *boardinfopropnames[] = { "MOUNT_TYPE", "MOTOR_CONTROLLER", "MOUNT_CODE" };

    InquireBoardVersion(boardinfo);
    nprop             = 3;
    // should test this is ok
    IUUpdateText(boardTP, boardinfo, (char **)boardinfopropnames, nprop);
    IDSetText(boardTP, nullptr);
    LOGF_DEBUG("%s(): MountCode = %d, MCVersion = %lx, setting minperiods Axis1=%d Axis2=%d",
               __FUNCTION__, MountCode, MCVersion, minperiods[Axis1], minperiods[Axis2]);
    /* Check supported mounts here */
    /*if ((MountCode == 0x80) || (MountCode == 0x81) || (MountCode == 0x82) || (MountCode == 0x90)) {

    throw EQModError(EQModError::ErrDisconnect, "Mount not supported %s (mount code %d)",
             boardinfo[0], MountCode);
    }
    */
    free(boardinfo[0]);
    free(boardinfo[1]);
    free(boardinfo[2]);
}

void Skywatcher::InquireBoardVersion(INDI::PropertyText boardTP)
{
    unsigned nprop             = 0;
    char *boardinfo[3];
    const char *boardinfopropnames[] = { "MOUNT_TYPE", "MOTOR_CONTROLLER", "MOUNT_CODE" };

    InquireBoardVersion(boardinfo);
    nprop             = 3;
    // should test this is ok
    boardTP.update(boardinfo, (char **)boardinfopropnames, nprop);
    boardTP.apply();
    LOGF_DEBUG("%s(): MountCode = %d, MCVersion = %lx, setting minperiods Axis1=%d Axis2=%d",
               __FUNCTION__, MountCode, MCVersion, minperiods[Axis1], minperiods[Axis2]);
    /* Check supported mounts here */
    /*if ((MountCode == 0x80) || (MountCode == 0x81) || (MountCode == 0x82) || (MountCode == 0x90)) {

    throw EQModError(EQModError::ErrDisconnect, "Mount not supported %s (mount code %d)",
             boardinfo[0], MountCode);
    }
    */
    free(boardinfo[0]);
    free(boardinfo[1]);
    free(boardinfo[2]);
}

void Skywatcher::InquireBoardVersion(char **boardinfo)
{

    /*
    uint32_t tmpMCVersion = 0;

    dispatch_command(InquireMotorBoardVersion, Axis1, nullptr);
    //read_eqmod();
    tmpMCVersion=Revu24str2long(response+1);
    MCVersion = ((tmpMCVersion & 0xFF) << 16) | ((tmpMCVersion & 0xFF00)) | ((tmpMCVersion & 0xFF0000) >> 16);
    MountCode=MCVersion & 0xFF;
    */
    minperiods[Axis1] = 6;
    minperiods[Axis2] = 6;
    //  strcpy(boardinfopropnames[0],"MOUNT_TYPE");
    boardinfo[0] = (char *)malloc(20 * sizeof(char));
    switch (MountCode)
    {
        case 0x00:
            strcpy(boardinfo[0], "EQ6");
            break;
        case 0x01:
            strcpy(boardinfo[0], "HEQ5");
            break;
        case 0x02:
            strcpy(boardinfo[0], "EQ5");
            break;
        case 0x03:
            strcpy(boardinfo[0], "EQ3");
            break;
        case 0x04:
            strcpy(boardinfo[0], "EQ8");
            break;
        case 0x05:
            strcpy(boardinfo[0], "AZEQ6");
            break;
        case 0x06:
            strcpy(boardinfo[0], "AZEQ5");
            break;
        case 0x0A:
            strcpy(boardinfo[0], "Star Adventurer");
            break;
        case 0x0C:
            strcpy(boardinfo[0], "Star Adventurer GTi");
            break;
        case 0x20:
            strcpy(boardinfo[0], "EQ8-R Pro");
            break;
        case 0x22:
            strcpy(boardinfo[0], "AZEQ6 Pro");
            break;
        case 0x23:
            strcpy(boardinfo[0], "EQ6-R Pro");
            break;
        case 0x25:
            strcpy(boardinfo[0], "CQ350 Pro");
            break;
        case 0x31:
            strcpy(boardinfo[0], "EQ5 Pro");
            break;
        case 0x45:
            strcpy(boardinfo[0], "Wave 150i");
            break;
        case 0x80:
            strcpy(boardinfo[0], "GT");
            break;
        case 0x81:
            strcpy(boardinfo[0], "MF");
            break;
        case 0x82:
            strcpy(boardinfo[0], "114GT");
            break;
        case 0x90:
            strcpy(boardinfo[0], "DOB");
            break;
        case 0xA5:
            strcpy(boardinfo[0], "AZ-GTi");
            break;
        case 0xF0:
            strcpy(boardinfo[0], "GEEHALEL");
            minperiods[Axis1] = 13;
            minperiods[Axis2] = 16;
            break;
        default:
            strcpy(boardinfo[0], "CUSTOM");
            break;
    }

    boardinfo[1] = (char *)malloc(5);
    sprintf(boardinfo[1], "%04x", (MCVersion >> 8));
    boardinfo[1][4] = '\0';
    boardinfo[2] = (char *)malloc(5);
    sprintf(boardinfo[2], "0x%02X", MountCode);
    boardinfo[2][4] = '\0';
    
#ifdef EQMODE_EXT
    SetMountDependantParameter(MountCode);
#endif
}

#ifdef EQMODE_EXT
void Skywatcher::SetMountDependantParameter(uint32_t mountCode)
{
// RAHomeInitOffset in hour:   RA default Home position is defined as RAStepHome = RAStepInit + RAHomeInitOffset/24.*RASteps360 (in step)
// DEHomeInitOffset in degree: DE default Home position is defined as DEStepHome = DEStepInit + DEHomeInitOffset/360.*DESteps360 (in step)

    // default settings

    RAHomeInitOffset = 0.;
    DEHomeInitOffset = 90.;
    RAStepInit = 0x800000;
    DEStepInit = 0x800000;
    //LOGF_INFO("modifOC SetMountDependantParameter %X %d %d\n",mountCode,RAStepInit,DEStepInit);
    // other settings
    switch (mountCode)
    {
        case 0x45: // Wave150i
            RAHomeInitOffset = -6.;
            break;
    }
}
#endif
void Skywatcher::InquireFeatures()
{
    uint32_t rafeatures = 0, defeatures = 0;
    try
    {
        GetFeature(Axis1, GET_FEATURES_CMD);
        rafeatures = Revu24str2long(response + 1);
        GetFeature(Axis2, GET_FEATURES_CMD);
        defeatures = Revu24str2long(response + 1);
    }
    catch (EQModError e)
    {
        LOGF_DEBUG("%s(): Mount does not support query features  (%c command)", __FUNCTION__,
                   GetFeatureCmd);
        rafeatures = 0;
        defeatures = 0;
    }
    if ((rafeatures & 0x000000F0) != (defeatures & 0x000000F0))
    {
        LOGF_WARN("%s(): Found different features for RA (%d) and DEC (%d)", __FUNCTION__,
                  rafeatures, defeatures);
    }
    if (rafeatures & 0x00000010)
    {
        LOGF_WARN("%s(): Found RA PPEC training on", __FUNCTION__);
    }
    if (defeatures & 0x00000010)
    {
        LOGF_WARN("%s(): Found DE PPEC training on", __FUNCTION__);
    }
    AxisFeatures[Axis1].inPPECTraining         = rafeatures & 0x00000010;
    AxisFeatures[Axis1].inPPEC                 = rafeatures & 0x00000020;
    AxisFeatures[Axis1].hasEncoder             = rafeatures & 0x00000001;
    AxisFeatures[Axis1].hasPPEC                = rafeatures & 0x00000002;
    AxisFeatures[Axis1].hasHomeIndexer         = rafeatures & 0x00000004;
    AxisFeatures[Axis1].isAZEQ                 = rafeatures & 0x00000008;
    AxisFeatures[Axis1].hasPolarLed            = rafeatures & 0x00001000;
    AxisFeatures[Axis1].hasCommonSlewStart     = rafeatures & 0x00002000; // supports :J3
    AxisFeatures[Axis1].hasHalfCurrentTracking = rafeatures & 0x00004000;
    AxisFeatures[Axis1].hasWifi                = rafeatures & 0x00008000;
    AxisFeatures[Axis2].inPPECTraining         = defeatures & 0x00000010;
    AxisFeatures[Axis2].inPPEC                 = defeatures & 0x00000020;
    AxisFeatures[Axis2].hasEncoder             = defeatures & 0x00000001;
    AxisFeatures[Axis2].hasPPEC                = defeatures & 0x00000002;
    AxisFeatures[Axis2].hasHomeIndexer         = defeatures & 0x00000004;
    AxisFeatures[Axis2].isAZEQ                 = defeatures & 0x00000008;
    AxisFeatures[Axis2].hasPolarLed            = defeatures & 0x00001000;
    AxisFeatures[Axis2].hasCommonSlewStart     = defeatures & 0x00002000; // supports :J3
    AxisFeatures[Axis2].hasHalfCurrentTracking = defeatures & 0x00004000;
    AxisFeatures[Axis2].hasWifi                = defeatures & 0x00008000;
}

bool Skywatcher::HasHomeIndexers()
{
    return (AxisFeatures[Axis1].hasHomeIndexer) && (AxisFeatures[Axis2].hasHomeIndexer);
}

bool Skywatcher::HasAuxEncoders()
{
    return (AxisFeatures[Axis1].hasEncoder) && (AxisFeatures[Axis2].hasEncoder);
}

bool Skywatcher::HasPPEC()
{
    return AxisFeatures[Axis1].hasPPEC;
}

bool Skywatcher::HasSnapPort1()
{
    return MountCode == 0x04 ||  MountCode == 0x05 ||  MountCode == 0x06 ||  MountCode == 0x0A || MountCode == 0x0C || MountCode == 0x23
           || MountCode == 0xA5;
}

bool Skywatcher::HasSnapPort2()
{
    return MountCode == 0x06;
}

bool Skywatcher::HasPolarLed()
{
    return (AxisFeatures[Axis1].hasPolarLed) && (AxisFeatures[Axis2].hasPolarLed);
}

// deprecated
void Skywatcher::InquireRAEncoderInfo(INumberVectorProperty *encoderNP)
{
    double steppersvalues[3];
    const char *steppersnames[] = { "RASteps360", "RAStepsWorm", "RAHighspeedRatio" };
    InquireEncoderInfo(Axis1, steppersvalues);
    // should test this is ok
    IUUpdateNumber(encoderNP, steppersvalues, (char **)steppersnames, 3);
    IDSetNumber(encoderNP, nullptr);
}

void Skywatcher::InquireRAEncoderInfo(INDI::PropertyNumber encoderNP)
{
    double steppersvalues[3];
    const char *steppersnames[] = { "RASteps360", "RAStepsWorm", "RAHighspeedRatio" };
    InquireEncoderInfo(Axis1, steppersvalues);
    // should test this is ok
    encoderNP.update(steppersvalues, (char **)steppersnames, 3);
    encoderNP.apply();
}

// deprecated
void Skywatcher::InquireDEEncoderInfo(INumberVectorProperty *encoderNP)
{
    double steppersvalues[3];
    const char *steppersnames[] = { "DESteps360", "DEStepsWorm", "DEHighspeedRatio" };
    InquireEncoderInfo(Axis2, steppersvalues);
    IUUpdateNumber(encoderNP, steppersvalues, (char **)steppersnames, 3);
    IDSetNumber(encoderNP, nullptr);
}

void Skywatcher::InquireDEEncoderInfo(INDI::PropertyNumber encoderNP)
{
    double steppersvalues[3];
    const char *steppersnames[] = { "DESteps360", "DEStepsWorm", "DEHighspeedRatio" };
    InquireEncoderInfo(Axis2, steppersvalues);
    // should test this is ok
    encoderNP.update(steppersvalues, (char **)steppersnames, 3);
    encoderNP.apply();
}


void Skywatcher::InquireEncoderInfo(SkywatcherAxis axis, double *steppersvalues)
{
    
    uint32_t * Steps360       = nullptr;
    uint32_t * StepsWorm      = nullptr;
    uint32_t * HighspeedRatio = nullptr;
    
    if (axis == Axis1)
    {
      Steps360 = &RASteps360;
      StepsWorm = &RAStepsWorm;
      HighspeedRatio = &RAHighspeedRatio;
    }
    else
    {
      Steps360 = &DESteps360;
      StepsWorm = &DEStepsWorm;
      HighspeedRatio = &DEHighspeedRatio;
    }

    // Steps per 360 degrees
    dispatch_command(InquireGridPerRevolution, axis, nullptr);
    //read_eqmod();
    *Steps360        = Revu24str2long(response + 1);

    // Steps per Worm
    dispatch_command(InquireTimerInterruptFreq, axis, nullptr);
    //read_eqmod();
    *StepsWorm = Revu24str2long(response + 1);
    // There is a bug in the earlier version firmware(Before 2.00) of motor controller MC001.
    // Overwrite the GearRatio reported by the MC for 80GT mount and 114GT mount.
    if ((MCVersion & 0x0000FF) == 0x80)
    {
        LOGF_WARN("%s: forcing %sStepsWorm for 80GT Mount (%x in place of %x)", __FUNCTION__,
                  axis == Axis1 ? "RA" : "DE", 0x162B97, *StepsWorm);
        *StepsWorm = 0x162B97; // for 80GT mount
    }
    if ((MCVersion & 0x0000FF) == 0x82)
    {
        LOGF_WARN("%s: forcing %sStepsWorm for 114GT Mount (%x in place of %x)", __FUNCTION__,
                  axis == Axis1 ? "RA" : "DE", 0x205318, *StepsWorm);
        *StepsWorm = 0x205318; // for 114GT mount
    }
    // HEQ5 with firmware 106, use same rate as RA
    // drift correction = 1.00455,  64935/1.00455 = 64640 = 0xFC80
    if (MCVersion == 0x10601)
    {
        LOGF_WARN("%s: forcing %sStepsWorm for HEQ5 with firmware 106 (%x in place of %x)", __FUNCTION__,
                  axis == Axis1 ? "RA" : "DE", 0xFC80, *StepsWorm);
        *StepsWorm = 0xFC80;
    }


    // Highspeed Ratio
    dispatch_command(InquireHighSpeedRatio, axis, nullptr);
    //read_eqmod();
    //HighspeedRatio=Revu24str2long(response+1);
    *HighspeedRatio  = Highstr2long(response + 1);

    steppersvalues[0] = (double)(*Steps360);
    steppersvalues[1] = static_cast<double>(*StepsWorm);
    steppersvalues[2] = static_cast<double>(*HighspeedRatio);


    if (axis == Axis1)
      backlashperiod[Axis1] =
        (long)(((SKYWATCHER_STELLAR_DAY * (double)RAStepsWorm) / (double)RASteps360) / SKYWATCHER_BACKLASH_SPEED_RA);
    else
      backlashperiod[Axis2] =
        (long)(((SKYWATCHER_STELLAR_DAY * (double)DEStepsWorm) / (double)DESteps360) / SKYWATCHER_BACKLASH_SPEED_DE);
}

bool Skywatcher::IsRARunning()
{
    CheckMotorStatus(Axis1);
    LOGF_DEBUG("%s() = %s", __FUNCTION__, (RARunning ? "true" : "false"));
    return (RARunning);
}

bool Skywatcher::IsDERunning()
{
    CheckMotorStatus(Axis2);
    LOGF_DEBUG("%s() = %s", __FUNCTION__, (DERunning ? "true" : "false"));
    return (DERunning);
}

void Skywatcher::ReadMotorStatus(SkywatcherAxis axis)
{
    dispatch_command(GetAxisStatus, axis, nullptr);
    //read_eqmod();
    switch (axis)
    {
        case Axis1:
            RAInitialized = (response[3] & 0x01);
            RARunning     = (response[2] & 0x01);
            if (response[1] & 0x01)
                RAStatus.slewmode = SLEW;
            else
                RAStatus.slewmode = GOTO;
            if (response[1] & 0x02)
                RAStatus.direction = BACKWARD;
            else
                RAStatus.direction = FORWARD;
            if (response[1] & 0x04)
                RAStatus.speedmode = HIGHSPEED;
            else
                RAStatus.speedmode = LOWSPEED;
            break;
        case Axis2:
            DEInitialized = (response[3] & 0x01);
            DERunning     = (response[2] & 0x01);
            if (response[1] & 0x01)
                DEStatus.slewmode = SLEW;
            else
                DEStatus.slewmode = GOTO;
            if (response[1] & 0x02)
                DEStatus.direction = BACKWARD;
            else
                DEStatus.direction = FORWARD;
            if (response[1] & 0x04)
                DEStatus.speedmode = HIGHSPEED;
            else
                DEStatus.speedmode = LOWSPEED;
            break;
        default:
            break;
    }
    gettimeofday(&lastreadmotorstatus[axis], nullptr);

}

void Skywatcher::SlewRA(double rate)
{
    double absrate       = fabs(rate);
    uint32_t period = 0;
    bool useHighspeed    = false;
    SkywatcherAxisStatus newstatus;

    LOGF_DEBUG("%s() : rate = %g", __FUNCTION__, rate);

    if (RARunning && (RAStatus.slewmode == GOTO))
    {
        throw EQModError(EQModError::ErrInvalidCmd, "Can not slew while goto is in progress");
    }

    if ((absrate < get_min_rate()) || (absrate > get_max_rate()))
    {
        throw EQModError(EQModError::ErrInvalidParameter,
                         "Speed rate out of limits: %.2fx Sidereal (min=%.2f, max=%.2f)", absrate, MIN_RATE, MAX_RATE);
    }
    //if (MountCode != 0xF0) {
    if (absrate > SKYWATCHER_LOWSPEED_RATE)
    {
        absrate      = absrate / RAHighspeedRatio;
        useHighspeed = true;
    }
    //}
    period = static_cast<uint32_t>(((SKYWATCHER_STELLAR_DAY * RAStepsWorm) / static_cast<double>(RASteps360)) / absrate);
    if (rate >= 0.0)
        newstatus.direction = FORWARD;
    else
        newstatus.direction = BACKWARD;
    newstatus.slewmode = SLEW;
    if (useHighspeed)
        newstatus.speedmode = HIGHSPEED;
    else
        newstatus.speedmode = LOWSPEED;
    SetMotion(Axis1, newstatus);
    SetSpeed(Axis1, period);
    if (!RARunning)
        StartMotor(Axis1);
}

void Skywatcher::SlewDE(double rate)
{
    double absrate       = fabs(rate);
    uint32_t period = 0;
    bool useHighspeed    = false;
    SkywatcherAxisStatus newstatus;

    LOGF_DEBUG("%s() : rate = %g", __FUNCTION__, rate);

    if (DERunning && (DEStatus.slewmode == GOTO))
    {
        throw EQModError(EQModError::ErrInvalidCmd, "Can not slew while goto is in progress");
    }

    if ((absrate < get_min_rate()) || (absrate > get_max_rate()))
    {
        throw EQModError(EQModError::ErrInvalidParameter,
                         "Speed rate out of limits: %.2fx Sidereal (min=%.2f, max=%.2f)", absrate, MIN_RATE, MAX_RATE);
    }
    //if (MountCode != 0xF0) {
    if (absrate > SKYWATCHER_LOWSPEED_RATE)
    {
        absrate      = absrate / DEHighspeedRatio;
        useHighspeed = true;
    }
    //}
    period = (long)(((SKYWATCHER_STELLAR_DAY * (double)DEStepsWorm) / (double)DESteps360) / absrate);

    LOGF_DEBUG("Slewing DE at %.2f %.2f %x %f\n", rate, absrate, period,
               (((SKYWATCHER_STELLAR_DAY * (double)RAStepsWorm) / (double)RASteps360) / absrate));

    if (rate >= 0.0)
        newstatus.direction = FORWARD;
    else
        newstatus.direction = BACKWARD;
    newstatus.slewmode = SLEW;
    if (useHighspeed)
        newstatus.speedmode = HIGHSPEED;
    else
        newstatus.speedmode = LOWSPEED;
    SetMotion(Axis2, newstatus);
    SetSpeed(Axis2, period);
    if (!DERunning)
        StartMotor(Axis2);
}

void Skywatcher::SlewTo(int32_t deltaraencoder, int32_t deltadeencoder)
{
    SkywatcherAxisStatus newstatus;
    bool useHighSpeed        = false;
    uint32_t lowperiod = 18, lowspeedmargin = 20000, breaks = 400;
    /* highperiod = RA 450X DE (+5) 200x, low period 32x */

    LOGF_DEBUG("%s() : deltaRA = %d deltaDE = %d", __FUNCTION__, deltaraencoder, deltadeencoder);

    newstatus.slewmode = GOTO;
    if (deltaraencoder >= 0)
        newstatus.direction = FORWARD;
    else
        newstatus.direction = BACKWARD;
    if (deltaraencoder < 0)
        deltaraencoder = -deltaraencoder;
    if (deltaraencoder > static_cast<int32_t>(lowspeedmargin))
        useHighSpeed = true;
    else
        useHighSpeed = false;
    if (useHighSpeed)
        newstatus.speedmode = HIGHSPEED;
    else
        newstatus.speedmode = LOWSPEED;
    if (deltaraencoder > 0)
    {
        SetMotion(Axis1, newstatus);
        if (useHighSpeed)
            SetSpeed(Axis1, minperiods[Axis1]);
        else
            SetSpeed(Axis1, lowperiod);
        SetTarget(Axis1, deltaraencoder);
        if (useHighSpeed)
            breaks = ((deltaraencoder > 3200) ? 3200 : deltaraencoder / 10);
        else
            breaks = ((deltaraencoder > 200) ? 200 : deltaraencoder / 10);
        SetTargetBreaks(Axis1, breaks);
        StartMotor(Axis1);
    }

    if (deltadeencoder >= 0)
        newstatus.direction = FORWARD;
    else
        newstatus.direction = BACKWARD;
    if (deltadeencoder < 0)
        deltadeencoder = -deltadeencoder;
    if (deltadeencoder > static_cast<int32_t>(lowspeedmargin))
        useHighSpeed = true;
    else
        useHighSpeed = false;
    if (useHighSpeed)
        newstatus.speedmode = HIGHSPEED;
    else
        newstatus.speedmode = LOWSPEED;
    if (deltadeencoder > 0)
    {
        SetMotion(Axis2, newstatus);
        if (useHighSpeed)
            SetSpeed(Axis2, minperiods[Axis2]);
        else
            SetSpeed(Axis2, lowperiod);
        SetTarget(Axis2, deltadeencoder);
        if (useHighSpeed)
            breaks = ((deltadeencoder > 3200) ? 3200 : deltadeencoder / 10);
        else
            breaks = ((deltadeencoder > 200) ? 200 : deltadeencoder / 10);
        SetTargetBreaks(Axis2, breaks);
        StartMotor(Axis2);
    }
}

void Skywatcher::AbsSlewTo(uint32_t raencoder, uint32_t deencoder, bool raup, bool deup)
{
    SkywatcherAxisStatus newstatus;
    bool useHighSpeed = false;
    int32_t deltaraencoder, deltadeencoder;
    uint32_t lowperiod = 18, lowspeedmargin = 20000, breaks = 400;
    /* highperiod = RA 450X DE (+5) 200x, low period 32x */

    LOGF_DEBUG("%s() : absRA = %ld raup = %c absDE = %ld deup = %c", __FUNCTION__, static_cast<long>(raencoder),
               (raup ? '1' : '0'), static_cast<long>(deencoder), (deup ? '1' : '0'));

    deltaraencoder = static_cast<int32_t>(raencoder - RAStep);
    deltadeencoder = static_cast<int32_t>(deencoder - DEStep);

    newstatus.slewmode = GOTO;
    if (raup)
        newstatus.direction = FORWARD;
    else
        newstatus.direction = BACKWARD;
    if (deltaraencoder < 0)
        deltaraencoder = -deltaraencoder;
    if (deltaraencoder > static_cast<int32_t>(lowspeedmargin))
        useHighSpeed = true;
    else
        useHighSpeed = false;
    if (useHighSpeed)
        newstatus.speedmode = HIGHSPEED;
    else
        newstatus.speedmode = LOWSPEED;
    if (deltaraencoder > 0)
    {
        SetMotion(Axis1, newstatus);
        if (useHighSpeed)
            SetSpeed(Axis1, minperiods[Axis1]);
        else
            SetSpeed(Axis1, lowperiod);
        SetAbsTarget(Axis1, raencoder);
        if (useHighSpeed)
            breaks = ((deltaraencoder > 3200) ? 3200 : deltaraencoder / 10);
        else
            breaks = ((deltaraencoder > 200) ? 200 : deltaraencoder / 10);
        breaks = (raup ? (raencoder - breaks) : (raencoder + breaks));
        SetAbsTargetBreaks(Axis1, breaks);
        StartMotor(Axis1);
    }

    if (deup)
        newstatus.direction = FORWARD;
    else
        newstatus.direction = BACKWARD;
    if (deltadeencoder < 0)
        deltadeencoder = -deltadeencoder;
    if (deltadeencoder > static_cast<int32_t>(lowspeedmargin))
        useHighSpeed = true;
    else
        useHighSpeed = false;
    if (useHighSpeed)
        newstatus.speedmode = HIGHSPEED;
    else
        newstatus.speedmode = LOWSPEED;
    if (deltadeencoder > 0)
    {
        SetMotion(Axis2, newstatus);
        if (useHighSpeed)
            SetSpeed(Axis2, minperiods[Axis2]);
        else
            SetSpeed(Axis2, lowperiod);
        SetAbsTarget(Axis2, deencoder);
        if (useHighSpeed)
            breaks = ((deltadeencoder > 3200) ? 3200 : deltadeencoder / 10);
        else
            breaks = ((deltadeencoder > 200) ? 200 : deltadeencoder / 10);
        breaks = (deup ? (deencoder - breaks) : (deencoder + breaks));
        SetAbsTargetBreaks(Axis2, breaks);
        StartMotor(Axis2);
    }
}

void Skywatcher::SetRARate(double rate)
{
    double absrate       = fabs(rate);
    uint32_t period = 0;
    bool useHighspeed    = false;
    SkywatcherAxisStatus newstatus;

    LOGF_DEBUG("%s() : rate = %g", __FUNCTION__, rate);

    if ((absrate < get_min_rate()) || (absrate > get_max_rate()))
    {
        throw EQModError(EQModError::ErrInvalidParameter,
                         "Speed rate out of limits: %.2fx Sidereal (min=%.2f, max=%.2f)", absrate, MIN_RATE, MAX_RATE);
    }
    //if (MountCode != 0xF0) {
    if (absrate > SKYWATCHER_LOWSPEED_RATE)
    {
        absrate      = absrate / RAHighspeedRatio;
        useHighspeed = true;
    }
    //}
    period              = static_cast<uint32_t>(((SKYWATCHER_STELLAR_DAY * RAStepsWorm) / static_cast<double>
                          (RASteps360)) / absrate);
    newstatus.direction = ((rate >= 0.0) ? FORWARD : BACKWARD);
    //newstatus.slewmode=RAStatus.slewmode;
    newstatus.slewmode = SLEW;
    if (useHighspeed)
        newstatus.speedmode = HIGHSPEED;
    else
        newstatus.speedmode = LOWSPEED;
    ReadMotorStatus(Axis1);
    if (RARunning)
    {
        if (newstatus.speedmode != RAStatus.speedmode)
            throw EQModError(EQModError::ErrInvalidParameter,
                             "Can not change rate while motor is running (speedmode differs).");
        if (newstatus.direction != RAStatus.direction)
            throw EQModError(EQModError::ErrInvalidParameter,
                             "Can not change rate while motor is running (direction differs).");
    }
    SetMotion(Axis1, newstatus);
    SetSpeed(Axis1, period);
}

void Skywatcher::SetDERate(double rate)
{
    double absrate       = fabs(rate);
    uint32_t period = 0;
    bool useHighspeed    = false;
    SkywatcherAxisStatus newstatus;

    LOGF_DEBUG("%s() : rate = %g", __FUNCTION__, rate);

    if ((absrate < get_min_rate()) || (absrate > get_max_rate()))
    {
        throw EQModError(EQModError::ErrInvalidParameter,
                         "Speed rate out of limits: %.2fx Sidereal (min=%.2f, max=%.2f)", absrate, MIN_RATE, MAX_RATE);
    }
    //if (MountCode != 0xF0) {
    if (absrate > SKYWATCHER_LOWSPEED_RATE)
    {
        absrate      = absrate / DEHighspeedRatio;
        useHighspeed = true;
    }
    //}
    period              = static_cast<uint32_t>(((SKYWATCHER_STELLAR_DAY * DEStepsWorm) / static_cast<double>
                          (DESteps360)) / absrate);
    newstatus.direction = ((rate >= 0.0) ? FORWARD : BACKWARD);
    //newstatus.slewmode=DEStatus.slewmode;
    newstatus.slewmode = SLEW;
    if (useHighspeed)
        newstatus.speedmode = HIGHSPEED;
    else
        newstatus.speedmode = LOWSPEED;
    ReadMotorStatus(Axis2);
    if (DERunning)
    {
        if (newstatus.speedmode != DEStatus.speedmode)
            throw EQModError(EQModError::ErrInvalidParameter,
                             "Can not change rate while motor is running (speedmode differs).");
        if (newstatus.direction != DEStatus.direction)
            throw EQModError(EQModError::ErrInvalidParameter,
                             "Can not change rate while motor is running (direction differs).");
    }
    SetMotion(Axis2, newstatus);
    SetSpeed(Axis2, period);
}

void Skywatcher::StartRATracking(double trackspeed)
{
    double rate;
    if (trackspeed != 0.0)
        rate = trackspeed / SKYWATCHER_STELLAR_SPEED;
    else
        rate = 0.0;
    LOGF_DEBUG("%s() : trackspeed = %g arcsecs/s, computed rate = %g", __FUNCTION__, trackspeed,
               rate);
    if (rate != 0.0)
    {
        SetRARate(rate);
        if (!RARunning)
            StartMotor(Axis1);
    }
    else
        StopMotor(Axis1);
}

void Skywatcher::StartDETracking(double trackspeed)
{
    double rate;
    if (trackspeed != 0.0)
        rate = trackspeed / SKYWATCHER_STELLAR_SPEED;
    else
        rate = 0.0;
    LOGF_DEBUG("%s() : trackspeed = %g arcsecs/s, computed rate = %g", __FUNCTION__, trackspeed,
               rate);
    if (rate != 0.0)
    {
        SetDERate(rate);
        if (!DERunning)
            StartMotor(Axis2);
    }
    else
        StopMotor(Axis2);
}

void Skywatcher::SetSpeed(SkywatcherAxis axis, uint32_t period)
{
    char cmd[7];
    SkywatcherAxisStatus *currentstatus;

    DEBUGF(telescope->DBG_MOUNT, "%s() : Axis = %c -- period=%ld", __FUNCTION__, AxisCmd[axis], static_cast<long>(period));

    ReadMotorStatus(axis);
    if (axis == Axis1)
        currentstatus = &RAStatus;
    else
        currentstatus = &DEStatus;
    if ((currentstatus->speedmode == HIGHSPEED) && (period < minperiods[axis]))
    {
        LOGF_WARN("Setting axis %c period to minimum. Requested is %d, minimum is %d\n", AxisCmd[axis],
                  period, minperiods[axis]);
        period = minperiods[axis];
    }
    long2Revu24str(period, cmd);

    if ((axis == Axis1) && (RARunning && (currentstatus->slewmode == GOTO || currentstatus->speedmode == HIGHSPEED)))
        throw EQModError(EQModError::ErrInvalidParameter,
                         "Can not change speed while motor is running and in goto or highspeed slew.");
    if ((axis == Axis2) && (DERunning && (currentstatus->slewmode == GOTO || currentstatus->speedmode == HIGHSPEED)))
        throw EQModError(EQModError::ErrInvalidParameter,
                         "Can not change speed while motor is running and in goto or highspeed slew.");

    if (axis == Axis1)
        RAPeriod = period;
    else
        DEPeriod = period;
    dispatch_command(SetStepPeriod, axis, cmd);
    //read_eqmod();
}

void Skywatcher::SetTarget(SkywatcherAxis axis, uint32_t increment)
{
    char cmd[7];
    DEBUGF(telescope->DBG_MOUNT, "%s() : Axis = %c -- increment=%ld", __FUNCTION__, AxisCmd[axis],
           static_cast<long>(increment));
    long2Revu24str(increment, cmd);
    //IDLog("Setting target for axis %c  to %d\n", AxisCmd[axis], increment);
    dispatch_command(SetGotoTargetIncrement, axis, cmd);
    //read_eqmod();
    Target[axis] = increment;
}

void Skywatcher::SetTargetBreaks(SkywatcherAxis axis, uint32_t increment)
{
    char cmd[7];
    DEBUGF(telescope->DBG_MOUNT, "%s() : Axis = %c -- increment=%ld", __FUNCTION__, AxisCmd[axis],
           static_cast<long>(increment));
    long2Revu24str(increment, cmd);
    //IDLog("Setting target for axis %c  to %d\n", AxisCmd[axis], increment);
    dispatch_command(SetBreakPointIncrement, axis, cmd);
    //read_eqmod();
    TargetBreaks[axis] = increment;
}

void Skywatcher::SetAbsTarget(SkywatcherAxis axis, uint32_t target)
{
    char cmd[7];
    DEBUGF(telescope->DBG_MOUNT, "%s() : Axis = %c -- target=%ld", __FUNCTION__, AxisCmd[axis], static_cast<long>(target));
    long2Revu24str(target, cmd);
    //IDLog("Setting target for axis %c  to %d\n", AxisCmd[axis], increment);
    dispatch_command(SetGotoTarget, axis, cmd);
    //read_eqmod();
    Target[axis] = target;
}

void Skywatcher::SetAbsTargetBreaks(SkywatcherAxis axis, uint32_t breakstep)
{
    char cmd[7];
    DEBUGF(telescope->DBG_MOUNT, "%s() : Axis = %c -- breakstep=%ld", __FUNCTION__, AxisCmd[axis],
           static_cast<long>(breakstep));
    long2Revu24str(breakstep, cmd);
    //IDLog("Setting target for axis %c  to %d\n", AxisCmd[axis], increment);
    dispatch_command(SetBreakStep, axis, cmd);
    //read_eqmod();
    TargetBreaks[axis] = breakstep;
}

void Skywatcher::SetFeature(SkywatcherAxis axis, uint32_t command)
{
    char cmd[7];
    DEBUGF(telescope->DBG_MOUNT, "%s() : Axis = %c -- command=%ld", __FUNCTION__, AxisCmd[axis], static_cast<long>(command));
    long2Revu24str(command, cmd);
    //IDLog("Setting target for axis %c  to %d\n", AxisCmd[axis], increment);
    dispatch_command(SetFeatureCmd, axis, cmd);
    //read_eqmod();
}

void Skywatcher::GetFeature(SkywatcherAxis axis, uint32_t command)
{
    char cmd[7];
    DEBUGF(telescope->DBG_MOUNT, "%s() : Axis = %c -- command=%ld", __FUNCTION__, AxisCmd[axis], static_cast<long>(command));
    long2Revu24str(command, cmd);
    //IDLog("Setting target for axis %c  to %d\n", AxisCmd[axis], increment);
    dispatch_command(GetFeatureCmd, axis, cmd);
    //read_eqmod();
}

void Skywatcher::GetIndexer(SkywatcherAxis axis)
{
    GetFeature(axis, GET_INDEXER_CMD);
    lastreadIndexer[axis] = Revu24str2long(response + 1);
}

void Skywatcher::GetRAIndexer()
{
    GetIndexer(Axis1);
}

void Skywatcher::GetDEIndexer()
{
    GetIndexer(Axis2);
}

void Skywatcher::ResetIndexer(SkywatcherAxis axis)
{
    SetFeature(axis, RESET_HOME_INDEXER_CMD);
}

void Skywatcher::ResetRAIndexer()
{
    ResetIndexer(Axis1);
}

void Skywatcher::ResetDEIndexer()
{
    ResetIndexer(Axis2);
}

void Skywatcher::TurnEncoder(SkywatcherAxis axis, bool on)
{
    uint32_t command;
    if (on)
        command = ENCODER_ON_CMD;
    else
        command = ENCODER_OFF_CMD;
    SetFeature(axis, command);
}

void Skywatcher::TurnRAEncoder(bool on)
{
    TurnEncoder(Axis1, on);
}

void Skywatcher::TurnDEEncoder(bool on)
{
    TurnEncoder(Axis2, on);
}

uint32_t Skywatcher::ReadEncoder(SkywatcherAxis axis)
{
    dispatch_command(InquireAuxEncoder, axis, nullptr);
    //read_eqmod();
    return Revu24str2long(response + 1);
}

uint32_t Skywatcher::GetRAAuxEncoder()
{
    return ReadEncoder(Axis1);
}

uint32_t Skywatcher::GetDEAuxEncoder()
{
    return ReadEncoder(Axis2);
}
#ifdef EQMODE_EXT
uint32_t Skywatcher::GetRANorthEncoder()
{
// We need a strict reference to the north to set the goto displacement limits
    int64_t offset;
    int64_t north;
    offset = RAHomeInitOffset / 24 * RASteps360; // may be >0 or <0
    north = RAStepInit + offset;
    
    return static_cast<uint32_t>(north);
}
double Skywatcher::GetRAHomeInitOffset()
{
    return RAHomeInitOffset;
}
#endif

void Skywatcher::SetST4RAGuideRate(unsigned char r)
{
    SetST4GuideRate(Axis1, r);
}

void Skywatcher::SetST4DEGuideRate(unsigned char r)
{
    SetST4GuideRate(Axis2, r);
}

void Skywatcher::SetST4GuideRate(SkywatcherAxis axis, unsigned char r)
{
    char cmd[2];
    DEBUGF(telescope->DBG_MOUNT, "%s() : Axis = %c -- rate=%c", __FUNCTION__, AxisCmd[axis], r);
    cmd[0] = r;
    cmd[1] = '\0';
    //IDLog("Setting target for axis %c  to %d\n", AxisCmd[axis], increment);
    dispatch_command(SetST4GuideRateCmd, axis, cmd);
    //read_eqmod();
}

void Skywatcher::TurnPPECTraining(bool on)
{
    uint32_t command;
    if (on)
        command = START_PPEC_TRAINING_CMD;
    else
        command = STOP_PPEC_TRAINING_CMD;
    SetFeature(Axis1, command);
}

void Skywatcher::SetLEDBrightness(uint8_t value)
{
    char cmd[3] = {0};
    char hexa[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    cmd[0] = hexa[(value & 0xF0) >> 4];
    cmd[1] = hexa[(value & 0x0F)];
    try
    {
        dispatch_command(SetPolarScopeLED, Axis1, cmd);
    }
    catch (EQModError e)
    {
        DEBUGF(telescope->DBG_MOUNT, "%s(): Mount does not support led brightness  (%c command)", __FUNCTION__,
               SetPolarScopeLED);
    }
}

void Skywatcher::TurnPPEC(bool on)
{
    uint32_t command;
    if (on)
        command = TURN_PPEC_ON_CMD;
    else
        command = TURN_PPEC_OFF_CMD;
    try
    {
        SetFeature(Axis1, command);
    }
    catch(EQModError e)
    {
        if (on)
        {
            if (e.severity == EQModError::ErrCmdFailed && response[1] == '8')
                LOG_ERROR("Can't enable PEC: no PEC data");
            else
                LOGF_ERROR("Can't enable PEC: %s", e.message);
        }
        else
            LOG_ERROR(e.message);
    }
}

void Skywatcher::GetPPECStatus(bool *intraining, bool *inppec)
{
    uint32_t features = 0;
    GetFeature(Axis1, GET_FEATURES_CMD);
    features    = Revu24str2long(response + 1);
    *intraining = AxisFeatures[Axis1].inPPECTraining = features & 0x00000010;
    *inppec = AxisFeatures[Axis1].inPPEC = features & 0x00000020;
}

void Skywatcher::TurnSnapPort(SkywatcherAxis axis, bool on)
{
    char snapcmd[2] = "0";
    if (on)
        snapcmd[0] = '1';
    else
        snapcmd[0] = '0';
    snapportstatus[axis] = on;
    DEBUGF(telescope->DBG_MOUNT, "%s() : Axis = %c -- snap=%c", __FUNCTION__, AxisCmd[axis], snapcmd);
    dispatch_command(SetSnapPort, axis, snapcmd);
    //read_eqmod();
}

void Skywatcher::TurnSnapPort1(bool on)
{
    TurnSnapPort(Axis1, on);
}

void Skywatcher::TurnSnapPort2(bool on)
{
    TurnSnapPort(Axis2, on);
}

bool Skywatcher::GetSnapPort1Status()
{
    return snapportstatus[Axis1];
}

bool Skywatcher::GetSnapPort2Status()
{
    return snapportstatus[Axis2];
}



void Skywatcher::SetAxisPosition(SkywatcherAxis axis, uint32_t step)
{
    char cmd[7];
    DEBUGF(telescope->DBG_MOUNT, "%s() : Axis = %c -- step=%ld", __FUNCTION__, AxisCmd[axis], static_cast<long>(step));
    long2Revu24str(step, cmd);
    //IDLog("Setting target for axis %c  to %d\n", AxisCmd[axis], increment);
    dispatch_command(SetAxisPositionCmd, axis, cmd);
    //read_eqmod();
}

void Skywatcher::SetRAAxisPosition(uint32_t step)
{
    SetAxisPosition(Axis1, step);
}

void Skywatcher::SetDEAxisPosition(uint32_t step)
{
    SetAxisPosition(Axis2, step);
}

void Skywatcher::StartMotor(SkywatcherAxis axis)
{
    bool usebacklash       = UseBacklash[axis];
    uint32_t backlash = Backlash[axis];
    DEBUGF(telescope->DBG_MOUNT, "%s() : Axis = %c", __FUNCTION__, AxisCmd[axis]);

    if (usebacklash)
    {
        LOGF_INFO("Checking backlash compensation for axis %c", AxisCmd[axis]);
        if (NewStatus[axis].direction != LastRunningStatus[axis].direction)
        {
            uint32_t currentsteps;
            char cmd[7];
            char motioncmd[3] = "20";                                               // lowspeed goto
            motioncmd[1]      = (NewStatus[axis].direction == FORWARD ? '0' : '1'); // same direction
            bool *motorrunning;
            struct timespec wait;
            LOGF_INFO("Performing backlash compensation for axis %c, microsteps = %d", AxisCmd[axis],
                      backlash);
            // Axis Position
            dispatch_command(GetAxisPosition, axis, nullptr);
            //read_eqmod();
            currentsteps = Revu24str2long(response + 1);
            // Backlash Speed
            long2Revu24str(backlashperiod[axis], cmd);
            dispatch_command(SetStepPeriod, axis, cmd);
            //read_eqmod();
            // Backlash motion mode
            dispatch_command(SetMotionMode, axis, motioncmd);
            //read_eqmod();
            // Target for backlash
            long2Revu24str(backlash, cmd);
            dispatch_command(SetGotoTargetIncrement, axis, cmd);
            //read_eqmod();
            // Target breaks for backlash (no break steps)
            long2Revu24str(backlash / 10, cmd);
            dispatch_command(SetBreakPointIncrement, axis, cmd);
            //read_eqmod();
            // Start Backlash
            dispatch_command(StartMotion, axis, nullptr);
            //read_eqmod();
            // Wait end of backlash
            if (axis == Axis1)
                motorrunning = &RARunning;
            else
                motorrunning = &DERunning;
            wait.tv_sec  = 0;
            wait.tv_nsec = 100000000; // 100ms
            ReadMotorStatus(axis);
            while (*motorrunning)
            {
                nanosleep(&wait, nullptr);
                ReadMotorStatus(axis);
            }
            // Restore microsteps
            long2Revu24str(currentsteps, cmd);
            dispatch_command(SetAxisPositionCmd, axis, cmd);
            //read_eqmod();
            // Restore Speed
            long2Revu24str((axis == Axis1 ? RAPeriod : DEPeriod), cmd);
            dispatch_command(SetStepPeriod, axis, cmd);
            //read_eqmod();
            // Restore motion mode
            switch (NewStatus[axis].slewmode)
            {
                case SLEW:
                    if (NewStatus[axis].speedmode == LOWSPEED)
                        motioncmd[0] = '1';
                    else
                        motioncmd[0] = '3';
                    break;
                case GOTO:
                    if (NewStatus[axis].speedmode == LOWSPEED)
                        motioncmd[0] = '2';
                    else
                        motioncmd[0] = '0';
                    break;
                default:
                    motioncmd[0] = '1';
                    break;
            }
            dispatch_command(SetMotionMode, axis, motioncmd);
            //read_eqmod();
            // Restore Target
            long2Revu24str(Target[axis], cmd);
            dispatch_command(SetGotoTargetIncrement, axis, cmd);
            //read_eqmod();
            // Restore Target breaks
            long2Revu24str(TargetBreaks[axis], cmd);
            dispatch_command(SetBreakPointIncrement, axis, cmd);
            //read_eqmod();
        }
    }
    dispatch_command(StartMotion, axis, nullptr);
    //read_eqmod();
}

void Skywatcher::StopRA()
{
    LOGF_DEBUG("%s() : calling RA StopWaitMotor", __FUNCTION__);
    StopWaitMotor(Axis1);
}

void Skywatcher::StopDE()
{
    LOGF_DEBUG("%s() : calling DE StopWaitMotor", __FUNCTION__);
    StopWaitMotor(Axis2);
}

void Skywatcher::SetMotion(SkywatcherAxis axis, SkywatcherAxisStatus newstatus)
{
    char motioncmd[3];
    SkywatcherAxisStatus *currentstatus;

    DEBUGF(telescope->DBG_MOUNT, "%s() : Axis = %c -- dir=%s mode=%s speedmode=%s", __FUNCTION__, AxisCmd[axis],
           ((newstatus.direction == FORWARD) ? "forward" : "backward"),
           ((newstatus.slewmode == SLEW) ? "slew" : "goto"),
           ((newstatus.speedmode == LOWSPEED) ? "lowspeed" : "highspeed"));

    CheckMotorStatus(axis);
    if (axis == Axis1)
        currentstatus = &RAStatus;
    else
        currentstatus = &DEStatus;
    motioncmd[2] = '\0';
    switch (newstatus.slewmode)
    {
        case SLEW:
            if (newstatus.speedmode == LOWSPEED)
                motioncmd[0] = '1';
            else
                motioncmd[0] = '3';
            break;
        case GOTO:
            if (newstatus.speedmode == LOWSPEED)
                motioncmd[0] = '2';
            else
                motioncmd[0] = '0';
            break;
        default:
            motioncmd[0] = '1';
            break;
    }
    if (newstatus.direction == FORWARD)
        motioncmd[1] = '0';
    else
        motioncmd[1] = '1';
    /*
    #ifdef STOP_WHEN_MOTION_CHANGED
    StopWaitMotor(axis);
    dispatch_command(SetMotionMode, axis, motioncmd);
    //read_eqmod();
    #else
    */
    if ((newstatus.direction != currentstatus->direction) || (newstatus.speedmode != currentstatus->speedmode) ||
            (newstatus.slewmode != currentstatus->slewmode))
    {
        StopWaitMotor(axis);
        dispatch_command(SetMotionMode, axis, motioncmd);
        //read_eqmod();
    }
    //#endif
    NewStatus[axis] = newstatus;
}

void Skywatcher::ResetMotions()
{
    char motioncmd[3];
    SkywatcherAxisStatus newstatus;

    DEBUGF(telescope->DBG_MOUNT, "%s() ", __FUNCTION__);

    motioncmd[2] = '\0';
    //set to SLEW/LOWSPEED
    newstatus.slewmode  = SLEW;
    newstatus.speedmode = LOWSPEED;
    motioncmd[0]        = '1';
    // Keep directions
    CheckMotorStatus(Axis1);
    newstatus.direction = RAStatus.direction;
    if (RAStatus.direction == FORWARD)
        motioncmd[1] = '0';
    else
        motioncmd[1] = '1';
    dispatch_command(SetMotionMode, Axis1, motioncmd);
    //read_eqmod();
    NewStatus[Axis1] = newstatus;

    CheckMotorStatus(Axis2);
    newstatus.direction = DEStatus.direction;
    if (DEStatus.direction == FORWARD)
        motioncmd[1] = '0';
    else
        motioncmd[1] = '1';
    dispatch_command(SetMotionMode, Axis2, motioncmd);
    //read_eqmod();
    NewStatus[Axis2] = newstatus;
}

void Skywatcher::StopMotor(SkywatcherAxis axis)
{
    ReadMotorStatus(axis);
    if (axis == Axis1 && RARunning)
        LastRunningStatus[Axis1] = RAStatus;
    if (axis == Axis2 && DERunning)
        LastRunningStatus[Axis2] = DEStatus;
    DEBUGF(telescope->DBG_MOUNT, "%s() : Axis = %c", __FUNCTION__, AxisCmd[axis]);
    dispatch_command(NotInstantAxisStop, axis, nullptr);
    //read_eqmod();
}

void Skywatcher::InstantStopMotor(SkywatcherAxis axis)
{
    ReadMotorStatus(axis);
    if (axis == Axis1 && RARunning)
        LastRunningStatus[Axis1] = RAStatus;
    if (axis == Axis2 && DERunning)
        LastRunningStatus[Axis2] = DEStatus;
    DEBUGF(telescope->DBG_MOUNT, "%s() : Axis = %c", __FUNCTION__, AxisCmd[axis]);
    dispatch_command(InstantAxisStop, axis, nullptr);
    //read_eqmod();
}

void Skywatcher::StopWaitMotor(SkywatcherAxis axis)
{
    bool *motorrunning;
    struct timespec wait;
    ReadMotorStatus(axis);
    if (axis == Axis1 && RARunning)
        LastRunningStatus[Axis1] = RAStatus;
    if (axis == Axis2 && DERunning)
        LastRunningStatus[Axis2] = DEStatus;
    DEBUGF(telescope->DBG_MOUNT, "%s() : Axis = %c", __FUNCTION__, AxisCmd[axis]);
    dispatch_command(NotInstantAxisStop, axis, nullptr);
    //read_eqmod();
    if (axis == Axis1)
        motorrunning = &RARunning;
    else
        motorrunning = &DERunning;
    wait.tv_sec  = 0;
    wait.tv_nsec = 100000000; // 100ms
    ReadMotorStatus(axis);
    while (*motorrunning)
    {
        nanosleep(&wait, nullptr);
        ReadMotorStatus(axis);
    }
}

/* Utilities */

void Skywatcher::CheckMotorStatus(SkywatcherAxis axis)
{
    struct timeval now;
    DEBUGF(telescope->DBG_SCOPE_STATUS, "%s() : Axis = %c", __FUNCTION__, AxisCmd[axis]);
    gettimeofday(&now, nullptr);
    if (((now.tv_sec - lastreadmotorstatus[axis].tv_sec) + ((now.tv_usec - lastreadmotorstatus[axis].tv_usec) / 1e6)) >
            SKYWATCHER_MAXREFRESH)
        ReadMotorStatus(axis);
}

double Skywatcher::get_min_rate()
{
    return MIN_RATE;
}

double Skywatcher::get_max_rate()
{
    return MAX_RATE;
}

bool Skywatcher::dispatch_command(SkywatcherCommand cmd, SkywatcherAxis axis, char *command_arg)
{
    for (uint8_t i = 0; i < EQMOD_MAX_RETRY; i++)
    {
        // Clear string
        command[0] = '\0';

        if (command_arg == nullptr)
            snprintf(command, SKYWATCHER_MAX_CMD, "%c%c%c%c", SkywatcherLeadingChar, cmd, AxisCmd[axis], SkywatcherTrailingChar);
        else
            snprintf(command, SKYWATCHER_MAX_CMD, "%c%c%c%s%c", SkywatcherLeadingChar, cmd, AxisCmd[axis], command_arg,
                     SkywatcherTrailingChar);

        int nbytes_written = 0;
        if (!isSimulation())
        {
            int err_code = 0;
            tcflush(PortFD, TCIOFLUSH);

            if ((err_code = tty_write_string(PortFD, command, &nbytes_written)) != TTY_OK)
            {
                if (i == EQMOD_MAX_RETRY - 1)
                {
                    char ttyerrormsg[ERROR_MSG_LENGTH];
                    tty_error_msg(err_code, ttyerrormsg, ERROR_MSG_LENGTH);
                    throw EQModError(EQModError::ErrDisconnect, "tty write failed, check connection: %s", ttyerrormsg);
                }
                else
                {
                    struct timespec wait;
                    wait.tv_sec  = 0;
                    wait.tv_nsec = 100000000; // 100ms
                    nanosleep(nullptr, &wait);
                    continue;
                }
            }
        }
        else
        {
            telescope->simulator->receive_cmd(command, &nbytes_written);
        }

        //if (INDI::Logger::debugSerial(cmd)) {
        command[nbytes_written - 1] = '\0'; //hmmm, remove \r, the  SkywatcherTrailingChar
        //DEBUGF(telescope->DBG_COMM, "dispatch_command: \"%s\", %d bytes written", command, nbytes_written);
        debugnextread = true;

        try
        {
            if (read_eqmod())
            {
                if (i > 0)
                {
                    LOGF_WARN("%s() : serial port read failed for %dms (%d retries), verify mount link.", __FUNCTION__,
                              (i * EQMOD_TIMEOUT) / 1000, i);
                }
                return true;
            }
        }
        catch (EQModError ex)
        {
            DEBUGF(telescope->DBG_COMM, "read_eqmod() failed: %s (attempt %i)", ex.message, i);
            // By this time, we just rethrow the error
            // JM 2018-05-07 immediately rethrow if GET_FEATURES_CMD
            if (i == EQMOD_MAX_RETRY - 1 || cmd == GetFeatureCmd)
                throw;
        }

        DEBUG(telescope->DBG_COMM, "read error, will retry again...");
    }

    return true;
}

bool Skywatcher::read_eqmod()
{
    int err_code = 0, nbytes_read = 0;

    // Clear string
    response[0] = '\0';
    if (!isSimulation())
    {
        //Have to onsider cases when we read ! (error) or 0x01 (buffer overflow)
        // Read until encountring a CR
        if ((err_code = tty_read_section_expanded(PortFD, response, 0x0D, 0, EQMOD_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            char ttyerrormsg[ERROR_MSG_LENGTH];
            tty_error_msg(err_code, ttyerrormsg, ERROR_MSG_LENGTH);
            throw EQModError(EQModError::ErrDisconnect, "tty read failed, check connection: %s", ttyerrormsg);
            //return false;
        }
    }
    else
    {
        telescope->simulator->send_reply(response, &nbytes_read);
    }
    // Remove CR
    response[nbytes_read - 1] = '\0';
    if (debugnextread)
    {
        DEBUGF(telescope->DBG_COMM, "read_eqmod: \"%s\", %d bytes read", response, nbytes_read);
        debugnextread = false;
    }
    switch (response[0])
    {
        case '=':
	    //check if response is valid
	    for (const char *p = &response[1]; *p != '\0'; ++p)
	    {
		//only allow uppercase hex chars
		if (!(isxdigit(*p) && !islower(*p)))
		{
            		throw EQModError(EQModError::ErrInvalidCmd, "Invalid response to command %s - Reply %s (response contains non-hex character)", command, response);
		}
	    }
            break;
        case '!':
            throw EQModError(EQModError::ErrCmdFailed, "Failed command %s - Reply %s", command, response);
        default:
            throw EQModError(EQModError::ErrInvalidCmd, "Invalid response to command %s - Reply %s", command, response);
    }

    return true;
}

uint32_t Skywatcher::Revu24str2long(char *s)
{
    uint32_t res = 0;
    res               = HEX(s[4]);
    res <<= 4;
    res |= HEX(s[5]);
    res <<= 4;
    res |= HEX(s[2]);
    res <<= 4;
    res |= HEX(s[3]);
    res <<= 4;
    res |= HEX(s[0]);
    res <<= 4;
    res |= HEX(s[1]);
    return res;
}

uint32_t Skywatcher::Highstr2long(char *s)
{
    uint32_t res = 0;
    res               = HEX(s[0]);
    res <<= 4;
    res |= HEX(s[1]);
    return res;
}

void Skywatcher::long2Revu24str(uint32_t n, char *str)
{
    char hexa[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    str[0]        = hexa[(n & 0xF0) >> 4];
    str[1]        = hexa[(n & 0x0F)];
    str[2]        = hexa[(n & 0xF000) >> 12];
    str[3]        = hexa[(n & 0x0F00) >> 8];
    str[4]        = hexa[(n & 0xF00000) >> 20];
    str[5]        = hexa[(n & 0x0F0000) >> 16];
    str[6]        = '\0';
}

// Park

// Backlash
void Skywatcher::SetBacklashRA(uint32_t backlash)
{
    Backlash[Axis1] = backlash;
}

void Skywatcher::SetBacklashUseRA(bool usebacklash)
{
    UseBacklash[Axis1] = usebacklash;
}
void Skywatcher::SetBacklashDE(uint32_t backlash)
{
    Backlash[Axis2] = backlash;
}

void Skywatcher::SetBacklashUseDE(bool usebacklash)
{
    UseBacklash[Axis2] = usebacklash;
}
