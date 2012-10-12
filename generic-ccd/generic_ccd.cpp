#if 0
    Generic CCD
    CCD Template for INDI Developers
    Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

#endif

#include <memory>
#include <time.h>
#include <math.h>
#include <sys/time.h>

#include "indidevapi.h"
#include "eventloop.h"

#include "generic_ccd.h"

#define MAX_CCD_TEMP	45		/* Max CCD temperature */
#define MIN_CCD_TEMP	-55		/* Min CCD temperature */
#define MAX_X_BIN	16		/* Max Horizontal binning */
#define MAX_Y_BIN	16		/* Max Vertical binning */
#define MAX_PIXELS	4096		/* Max number of pixels in one dimension */
#define POLLMS		1000		/* Polling time (ms) */
#define TEMP_THRESHOLD  .25		/* Differential temperature threshold (C)*/

std::auto_ptr<GenericCCD> genericCCD(0);

 void ISInit()
 {
    static int isInit =0;

    if (isInit == 1)
        return;

     isInit = 1;
     if(genericCCD.get() == 0) genericCCD.reset(new GenericCCD());

 }

 void ISGetProperties(const char *dev)
 {
         ISInit();
         genericCCD->ISGetProperties(dev);
 }

 void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
 {
         ISInit();
         genericCCD->ISNewSwitch(dev, name, states, names, num);
 }

 void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
 {
         ISInit();
         genericCCD->ISNewText(dev, name, texts, names, num);
 }

 void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
 {
         ISInit();
         genericCCD->ISNewNumber(dev, name, values, names, num);
 }

 void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
 {
   INDI_UNUSED(dev);
   INDI_UNUSED(name);
   INDI_UNUSED(sizes);
   INDI_UNUSED(blobsizes);
   INDI_UNUSED(blobs);
   INDI_UNUSED(formats);
   INDI_UNUSED(names);
   INDI_UNUSED(n);
 }
 void ISSnoopDevice (XMLEle *root)
 {
     ISInit();
     genericCCD->ISSnoopDevice(root);
 }


GenericCCD::GenericCCD()
{
   sim = false;
}


GenericCCD::~GenericCCD()
{

}

const char * GenericCCD::getDefaultName()
{
    return (char *)"Generic CCD";
}

bool GenericCCD::initProperties()
{
    // Init parent properties first
    INDI::CCD::initProperties();

    IUFillSwitch(&ResetS[0], "RESET", "Reset", ISS_OFF);
    IUFillSwitchVector(&ResetSP, ResetS, 1, getDeviceName(), "FRAME_RESET", "Frame Values", IMAGE_SETTINGS_TAB, IP_WO, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&TemperatureN[0], "CCD_TEMPERATURE_VALUE", "Temperature (C)", "%5.2f", MIN_CCD_TEMP, MAX_CCD_TEMP, 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "CCD_TEMPERATURE", "Temperature", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

}


void GenericCCD::ISGetProperties(const char *dev)
{
    INDI::CCD::ISGetProperties(dev);


    // Add Debug, Simulator, and Configuration controls
    addAuxControls();
}

bool GenericCCD::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        defineNumber(&TemperatureNP);
        defineSwitch(&ResetSP);

        // Let's get parameters now from CCD
        setupParams();

        timerID = SetTimer(POLLMS);
    }
    else
    {

        deleteProperty(TemperatureNP.name);
        deleteProperty(ResetSP.name);

        rmTimer(timerID);
    }

    return true;
}


bool GenericCCD::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{

    if(strcmp(dev,getDeviceName())==0)
    {

        /* Reset */
        if (!strcmp (name, ResetSP.name))
        {
          if (IUUpdateSwitch(&ResetSP, states, names, n) < 0) return false;
          resetFrame();
          return true;
        }

    }

    //  Nobody has claimed this, so, ignore it
    return INDI::CCD::ISNewSwitch(dev,name,states,names,n);
}

bool GenericCCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    INumber *np;

    if(strcmp(dev,getDeviceName())==0)
    {

        /* Temperature*/
        if (!strcmp(TemperatureNP.name, name))
        {
            TemperatureNP.s = IPS_IDLE;

            np = IUFindNumber(&TemperatureNP, names[0]);

            if (!np)
            {
                IDSetNumber(&TemperatureNP, "Unknown error. %s is not a member of %s property.", names[0], name);
                return false;
            }

            if (values[0] < MIN_CCD_TEMP || values[0] > MAX_CCD_TEMP)
            {
                IDSetNumber(&TemperatureNP, "Error: valid range of temperature is from %d to %d", MIN_CCD_TEMP, MAX_CCD_TEMP);
                return false;
            }

            /**********************************************************
             *
             *
             *
             *  IMPORRANT: Put here your CCD Set Temperature Function
             *
             *
             *
             **********************************************************/

            TemperatureRequest = values[0];
            TemperatureNP.s = IPS_BUSY;

            IDSetNumber(&TemperatureNP, "Setting CCD temperature to %+06.2f C", values[0]);
            if (isDebug())
                IDLog("Setting CCD temperature to %+06.2f C\n", values[0]);
            return true;
        }
    }

    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return INDI::CCD::ISNewNumber(dev,name,values,names,n);
}

bool GenericCCD::Connect()
{

    sim = isSimulation();

    if (sim)
        return true;
    else
    {
        IDMessage(getDeviceName(), "Generic CCD can only run in simulation mode, no hardware implementation yet!");
        return false;
    }

    IDMessage(getDeviceName(), "Attempting to find the Generic CCD...");

    if (isDebug())
    {
        IDLog ("Connecting CCD\n");
        IDLog("Attempting to find the camera\n");
    }

    /**********************************************************
     *
     *
     *
     *  IMPORRANT: Put here your CCD Connect function
     *  If you encounter an error, send the client a message
     *  e.g.
     *  IDMessage(getDeviceName(), "Error, unable to connect due to ...");
     *  return false;
     *
     *
     **********************************************************/

    /* Success! */
    IDMessage(getDeviceName(), "CCD is online. Retrieving basic data.");
    if (isDebug())
       IDLog("CCD is online. Retrieving basic data.\n");

        return true;
}


bool GenericCCD::Disconnect()
{
    if (sim)
        return true;

    /**********************************************************
     *
     *
     *
     *  IMPORRANT: Put here your CCD disonnect function
     *  If you encounter an error, send the client a message
     *  e.g.
     *  IDMessage(getDeviceName(), "Error, unable to disconnect due to ...");
     *  return false;
     *
     *
     **********************************************************/

    IDMessage(getDeviceName(), "CCD is offline.");
    return true;
}


bool GenericCCD::setupParams()
{

    if (isDebug())
        IDLog("In setupParams\n");

    float x_pixel_size, y_pixel_size;
    int bit_depth=16;
    int x_1,y_1,x_2,y_2;

    /**********************************************************
     *
     *
     *
     *  IMPORRANT: Get basic CCD parameters here such as
     *  + Pixel Size X
     *  + Pixel Size Y
     *  + Bit Depth?
     *  + X, Y, W, H of frame
     *  + Temperature
     *  + ...etc
     *
     *
     *
     **********************************************************/

    ///////////////////////////
    // 1. Get Pixel size
    ///////////////////////////
    if (sim)
    {
       x_pixel_size = 5.4;
       y_pixel_size = 5.4;
    }
    else
    {
        // Actucal CALL to CCD to get pixel size here
    }


    ///////////////////////////
    // 2. Get Frame
    ///////////////////////////
    if (sim)
    {
        x_1 = y_1 = 0;
        x_2 = 1280;
        y_2 = 1024;
    }
    else
    {
      // Actucal CALL to CCD to get frame information here
    }

    ///////////////////////////
    // 3. Get temperature
    ///////////////////////////
    if (sim)
        TemperatureN[0].value = 25.0;
    else
    {
      // Actucal CALL to CCD to get temperature here
    }

    IDMessage(getDeviceName(), "The CCD Temperature is %f.\n", TemperatureN[0].value);
    IDSetNumber(&TemperatureNP, NULL);

    if (isDebug())
        IDLog("The CCD Temperature is %f.\n", TemperatureN[0].value);

    if (sim)
        bit_depth = 16;
    else
    {
      // Set your actual CCD bit depth
    }


    SetCCDParams(x_2 - x_1, y_2 - y_1, bit_depth, x_pixel_size, y_pixel_size);

    if (sim)
        minDuration = 0.05;
    else
    {
        // Set your actual CCD minimum exposure duration
    }


    // Now we usually do the following in the hardware
    // Set Frame to LIGHT or NORMAL
    // Set Binning to 1x1
    /* Default frame type is NORMAL */


    // Let's calculate required buffer
    int nbuf;
    nbuf=PrimaryCCD.getXRes()*PrimaryCCD.getYRes() * PrimaryCCD.getBPP()/8;                 //  this is pixel count
    nbuf+=512;                      //  leave a little extra at the end
    PrimaryCCD.setFrameBufferSize(nbuf);

    return true;

}

int GenericCCD::StartExposure(float duration)
{

        bool shortExposure = false;

        if(duration < minDuration)
        {
            IDMessage(getDeviceName(), "Exposure shorter than minimum duration %g s requested. \n Setting exposure time to %g s.", duration,minDuration);
            duration = minDuration;
        }

        if (imageFrameType == CCDChip::BIAS_FRAME)
        {
            duration = minDuration;
            shortExposure=true;
            IDMessage(getDeviceName(), "Bias Frame (s) : %g\n", minDuration);
            if (isDebug())
                IDLog("Bias Frame (s) : %g\n", minDuration);
        }


        /**********************************************************
         *
         *
         *
         *  IMPORRANT: Put here your CCD start exposure here
         *  Please note that duration passed is in seconds.
         *  If there is an error, report it back to client
         *  e.g.
         *  IDMessage(getDeviceName(), "Error, unable to start exposure due to ...");
         *  return -1;
         *
         *
         **********************************************************/

        PrimaryCCD.setExposureDuration(duration);
        ExposureRequest = duration;
        if (isDebug())
             IDLog("Exposure Time (s) is: %g\n", duration);

        gettimeofday(&ExpStart,NULL);
        IDMessage(getDeviceName(), "Taking a %g seconds frame...", ExposureRequest);

        InExposure = true;
        return (shortExposure ? 1 : 0);
}


bool GenericCCD::AbortExposure()
{

    /**********************************************************
     *
     *
     *
     *  IMPORRANT: Put here your CCD abort exposure here
     *  If there is an error, report it back to client
     *  e.g.
     *  IDMessage(getDeviceName(), "Error, unable to abort exposure due to ...");
     *  return false;
     *
     *
     **********************************************************/

    InExposure=false;
    return true;
}

bool GenericCCD::updateCCDFrameType(CCDChip::CCD_FRAME fType)
{
    int err=0;
    CCDChip::CCD_FRAME imageFrameType = PrimaryCCD.getFrameType();

    if (fType == imageFrameType || sim)
        return true;

    switch (imageFrameType)
    {
        case CCDChip::BIAS_FRAME:
        case CCDChip::DARK_FRAME:
        /**********************************************************
         *
         *
         *
         *  IMPORRANT: Put here your CCD Frame type here
         *  BIAS and DARK are taken with shutter closed, so _usually_
         *  most CCD this is a call to let the CCD know next exposure shutter
         *  must be closed. Customize as appropiate for the hardware
         *  If there is an error, report it back to client
         *  e.g.
         *  IDMessage(getDeviceName(), "Error, unable to set frame type to ...");
         *  return false;
         *
         *
         **********************************************************/
        break;

       case CCDChip::LIGHT_FRAME:
       case CCDChip::FLAT_FRAME:
        /**********************************************************
         *
         *
         *
         *  IMPORRANT: Put here your CCD Frame type here
         *  LIGHT and FLAT are taken with shutter open, so _usually_
         *  most CCD this is a call to let the CCD know next exposure shutter
         *  must be open. Customize as appropiate for the hardware
         *  If there is an error, report it back to client
         *  e.g.
         *  IDMessage(getDeviceName(), "Error, unable to set frame type to ...");
         *  return false;
         *
         *
         **********************************************************/
        break;
    }

    PrimaryCCD.setFrameType(fType);

    return true;

}

bool GenericCCD::updateCCDFrame(int x, int y, int w, int h)
{
    /* Add the X and Y offsets */
    long x_1 = x;
    long y_1 = y;

    long bin_width  = x_1 + (w / PrimaryCCD.getBinX());
    long bin_height = y_1 + (h / PrimaryCCD.getBinY());

    if (bin_width > PrimaryCCD.getXRes() / PrimaryCCD.getBinX())
    {
        IDMessage(getDeviceName(), "Error: invalid width requested %d", w);
        return false;
    }
    else if (bin_height > PrimaryCCD.getYRes() / PrimaryCCD.getBinY())
    {
        IDMessage(getDeviceName(), "Error: invalid height request %d", h);
        return false;
    }

    if (isDebug())
        IDLog("The Final image area is (%ld, %ld), (%ld, %ld)\n", x_1, y_1, bin_width, bin_height);


    /**********************************************************
     *
     *
     *
     *  IMPORRANT: Put here your CCD Frame dimension call
     *  The values calculated above are BINNED width and height
     *  which is what most CCD APIs require, but in case your
     *  CCD API implementation is different, don't forget to change
     *  the above calculations.
     *  If there is an error, report it back to client
     *  e.g.
     *  IDMessage(getDeviceName(), "Error, unable to set frame to ...");
     *  return false;
     *
     *
     **********************************************************/

    // Set UNBINNED coords
    PrimaryCCD.setFrame(x_1, y_1, w,  h);

    int nbuf;
    nbuf=(bin_width*bin_height * PrimaryCCD.getBPP()/8);                 //  this is pixel count
    nbuf+=512;                      //  leave a little extra at the end
    PrimaryCCD.setFrameBufferSize(nbuf);

    if (isDebug())
        IDLog("Setting frame buffer size to %d bytes.\n", nbuf);

    return true;
}

bool GenericCCD::updateCCDBin(int binx, int biny)
{

    /**********************************************************
     *
     *
     *
     *  IMPORRANT: Put here your CCD Binning call
     *  If there is an error, report it back to client
     *  e.g.
     *  IDMessage(getDeviceName(), "Error, unable to set binning to ...");
     *  return false;
     *
     *
     **********************************************************/

    PrimaryCCD.setBin(binx, biny);

    return updateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
}

float GenericCCD::CalcTimeLeft()
{
    double timesince;
    double timeleft;
    struct timeval now;
    gettimeofday(&now,NULL);

    timesince=(double)(now.tv_sec * 1000.0 + now.tv_usec/1000) - (double)(ExpStart.tv_sec * 1000.0 + ExpStart.tv_usec/1000);
    timesince=timesince/1000;


    timeleft=ExposureRequest-timesince;
    return timeleft;
}

/* Downloads the image from the CCD.
 N.B. No processing is done on the image */
int GenericCCD::grabImage()
{
        char * image = PrimaryCCD.getFrameBuffer();
        int width = PrimaryCCD.getSubW() / PrimaryCCD.getBinX() * PrimaryCCD.getBPP()/8;
        int height = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();

        if (sim)
        {
            if (isDebug())
            {
                IDLog("GrabImage Width: %d - Height: %d\n", width, height);
                IDLog("Buf size: %d bytes.\n", width*height);
            }

            for (int i=0; i < height ; i++)
                for (int j=0; j < width; j++)
                    image[i*width+j] = rand() % 255;
        }
        else
        {
            /**********************************************************
             *
             *
             *
             *  IMPORRANT: Put here your CCD Get Image routine here
             *  use the image, width, and height variables above
             *  If there is an error, report it back to client
             *  e.g.
             *  IDMessage(getDeviceName(), "Error, unable to set binning to ...");
             *  return false;
             *
             *
             **********************************************************/
        }

        IDMessage(getDeviceName(), "Download complete.");

        if (isDebug())
            IDLog("Download complete.");

        ExposureComplete(&PrimaryCCD);

    return 0;
}

void GenericCCD::addFITSKeywords(fitsfile *fptr, CCDChip *targetChip)
{
    INDI::CCD::addFITSKeywords(fptr, targetChip);

    int status=0;
    fits_update_key_s(fptr, TDOUBLE, "CCD-TEMP", &(TemperatureN[0].value), "CCD Temperature (Celcius)", &status);
    fits_write_date(fptr, &status);

}


void GenericCCD::resetFrame()
{    
        updateCCDBin(1, 1);
        updateCCDFrame(0,0, PrimaryCCD.getXRes(), PrimaryCCD.getYRes());
        IUResetSwitch(&ResetSP);
        ResetSP.s = IPS_IDLE;
        IDSetSwitch(&ResetSP, "Resetting frame and binning.");

        return;
}

void GenericCCD::TimerHit()
{
    int timerID=-1;
    int err=0;
    long timeleft;
    double ccdTemp;

    if(isConnected() == false)
        return;  //  No need to reset timer if we are not connected anymore

    if (InExposure)
    {
        timeleft=CalcTimeLeft();

        if(timeleft < 1.0)
        {
            if(timeleft > 0.25)
            {
                //  a quarter of a second or more
                //  just set a tighter timer
                timerID = SetTimer(250);
            } else
            {
                if(timeleft >0.07)
                {
                    //  use an even tighter timer
                    timerID = SetTimer(50);
                } else
                {
                    //  it's real close now, so spin on it
                    while(!sim && timeleft > 0)
                    {

                        /**********************************************************
                         *
                         *  IMPORRANT: If supported by your CCD API
                         *  Add a call here to check if the image is ready for download
                         *  If image is ready, set timeleft to 0. Some CCDs (check FLI)
                         *  also return timeleft in msec.
                         *
                         **********************************************************/

                        int slv;
                        slv=100000*timeleft;
                        usleep(slv);
                    }

                    /* We're done exposing */
                    IDMessage(getDeviceName(), "Exposure done, downloading image...");

                    if (isDebug())
                        IDLog("Exposure done, downloading image...\n");

                    PrimaryCCD.setExposureLeft(0);
                    InExposure = false;
                    /* grab and save image */
                    grabImage();

                }
            }
        }
        else
        {

         if (isDebug())
         {
            IDLog("With time left %ld\n", timeleft);
            IDLog("image not yet ready....\n");
         }

         PrimaryCCD.setExposureLeft(timeleft);

        }

    }

    switch (TemperatureNP.s)
    {
      case IPS_IDLE:
      case IPS_OK:
        /**********************************************************
         *
         *
         *
         *  IMPORRANT: Put here your CCD Get temperature call here
         *  If there is an error, report it back to client
         *  e.g.
         *  IDMessage(getDeviceName(), "Error, unable to get temp due to ...");
         *  return false;
         *
         *
         **********************************************************/

        if (fabs(TemperatureN[0].value - ccdTemp) >= TEMP_THRESHOLD)
        {
          TemperatureN[0].value = ccdTemp;
          IDSetNumber(&TemperatureNP, NULL);
        }
        break;

      case IPS_BUSY:
        if (sim)
        {
            ccdTemp = TemperatureRequest;
            TemperatureN[0].value = ccdTemp;
        }
        else
        {
            /**********************************************************
             *
             *
             *
             *  IMPORRANT: Put here your CCD Get temperature call here
             *  If there is an error, report it back to client
             *  e.g.
             *  IDMessage(getDeviceName(), "Error, unable to get temp due to ...");
             *  return false;
             *
             *
             **********************************************************/
        }

        // If we're within threshold, let's make it BUSY ---> OK
        if (fabs(TemperatureRequest - ccdTemp) <= TEMP_THRESHOLD)
        {
            TemperatureNP.s = IPS_OK;
            IDSetNumber(&TemperatureNP, NULL);
        }

        TemperatureN[0].value = ccdTemp;
        IDSetNumber(&TemperatureNP, NULL);
        break;

      case IPS_ALERT:
        break;
    }

    if (timerID == -1)
        SetTimer(POLLMS);
    return;
}
