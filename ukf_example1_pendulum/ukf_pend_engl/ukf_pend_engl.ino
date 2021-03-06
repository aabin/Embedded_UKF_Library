#include <Wire.h>
#include <elapsedMillis.h>
#include "konfig.h"
#include "matrix.h"
#include "ukf.h"




/* In this example, we will simulate the damped pendulum:
 *  - The input u           : none
 *  - The state variable x  : [theta dtheta/dt]'    (angle and angular speed of the pendulum)
 *  - The output y          : [x y]'                (coordinate position of the ball)
 * 
 * We can model the pendulum system as (it is assume the rod is massless, without friction):
 *      (dtheta)^2/dt^2 = -g/l*sin(theta) -alpha*(dtheta/dt)
 *      
 *      where g = acceleration constant due to gravity, l = pendulum length, alpha = damping factor
 * 
 * where the coordinate position of the ball can be described by (0,0 coordinate is the pivot point):
 *      x =  sin(theta) * l
 *      y = -cos(theta) * l
 * 
 * *See https://en.wikipedia.org/wiki/Pendulum_(mathematics)#Simple_gravity_pendulum for undamped model, 
 * *See http://www.nld.ds.mpg.de/applets/pendulum/eqm1.htm or http://www.nld.ds.mpg.de/applets/pendulum/eqm2.htm for the damped model.
 * 
 * 
 * 
 * 
 * The model then can be described in state space formulation as:
 *  The state variables:
 *      x1 = theta        --> dx1/dt = dtheta/dt
 *      x2 = dtheta/dt    --> dx2/dt = (dtheta)^2)/dt^2
 *  The output variables:
 *      y1 = x
 *      y2 = y
 * 
 *  The update function in continuous time:
 *      dx1/dt = x2
 *      dx2/dt = -g/l * sin(x1)
 *  The update function in discrete time:
 *      x1(k+1) = x1(k) + x2(k)*dt
 *      x2(k+1) = x2(k) - g/l*sin(x1(k))*dt - damping_factor*x2*dt
 *  
 *  The output (in discrete time):
 *      y1(k) =  sin(x1(k)) * l
 *      y2(k) = -cos(x1(k)) * l
 * 
 */
#define pend_g      (9.81)          /* gravitation constant */
#define pend_l      (5)             /* length of the pendulum rod, in meters */
#define pend_alpha  (0.3)           /* damping factor */


/* Just example */
#define P_INIT      (100)
#define Rv_INIT     (0.01)
#define Rn_INIT     (1.)


bool Main_bUpdateNonlinearX(Matrix &X_Next, Matrix &X, Matrix &U);
bool Main_bUpdateNonlinearY(Matrix &Y, Matrix &X, Matrix &U);

elapsedMillis timerLed, timerUKF;
uint64_t u64compuTime;

Matrix X_true(SS_X_LEN, 1);
Matrix X_est_init(SS_X_LEN, 1);
Matrix Y(SS_Z_LEN, 1);
Matrix U(SS_U_LEN, 1);
UKF UKF_IMU(X_est_init, Main_bUpdateNonlinearX, Main_bUpdateNonlinearY, P_INIT, Rv_INIT, Rn_INIT);

char bufferTxSer[100];


void setup() {
    /* serial to display data */
    Serial.begin(115200);
    while(!Serial) {}
    
    
    
    /* For example, let's set the theta(k=0) = pi/2     (i.e. the pendulum rod is parallel with the horizontal plane) */
    X_true[0][0] = 3.14159265359/2.;
    
    /* Observe that we set the wrong initial x_estimated value!  (X_UKF(k=0) != X_TRUE(k=0)) */
    X_est_init[0][0] = -3.14159265359;
    
    UKF_IMU.vReset(X_est_init, P_INIT, Rv_INIT, Rn_INIT);
}

void loop() {
    if (timerUKF > SS_DT_MILIS) {
        /* ================== Read the sensor data / simulate the system here ================== */

        /*  The update function in discrete time:
         *      x1(k+1) = x1(k) + x2(k)*dt
         *      x2(k+1) = x2(k) - g/l*sin(x1(k))*dt - damping_factor*x2*dt
         */
        float_prec theta     = X_true[0][0];
        float_prec theta_dot = X_true[1][0];
        X_true[0][0] = theta + (theta_dot*SS_DT);   
        X_true[1][0] = theta_dot + (-pend_g/pend_l*sin(theta) - pend_alpha*theta_dot)*SS_DT;
        
        /*  The output (in discrete time):
         *      y1(k) = sin(x1(k))
         *      y2(k) = cos(x1(k))
         */
        theta = X_true[0][0];
        
        Y[0][0] =  sin(theta) * pend_l;
        Y[1][0] = -cos(theta) * pend_l;
        
        /* Let's add some noise! */
        Y[0][0] += (float((rand() % 20) - 10) / 10.);       /* add +/- 1 meters noise to x position */
        
        /* ------------------ Read the sensor data / simulate the system here ------------------ */
        
        
        
        /* ============================= Update the Kalman Filter ============================== */
        u64compuTime = micros();
        if (!UKF_IMU.bUpdate(Y, U)) {
            X_est_init.vIsiNol();
            UKF_IMU.vReset(X_est_init, P_INIT, Rv_INIT, Rn_INIT);
            Serial.println("Whoop ");
        }
        u64compuTime = (micros() - u64compuTime);
        /* ----------------------------- Update the Kalman Filter ------------------------------ */
        
        
        
        /* =========================== Print to serial (for plotting) ========================== */
        #if (0)
            /* Print: Computation time, x1 (without noise), x1 estimated */
            snprintf(bufferTxSer, sizeof(bufferTxSer)-1, "%.3f %.3f %.3f ", ((float)u64compuTime)/1000., X_true[0][0], UKF_IMU.GetX()[0][0]);
            Serial.print(bufferTxSer);
        #else
            /* Print: Computation time, y1 (with noise), y1 (without noise), y1 estimated */
            snprintf(bufferTxSer, sizeof(bufferTxSer)-1, "%.3f %.3f %.3f %.3f ", ((float)u64compuTime)/1000., Y[0][0], sin(X_true[0][0])*pend_l, sin(UKF_IMU.GetX()[0][0])*pend_l);
            Serial.print(bufferTxSer);
        #endif
        Serial.print('\n');
        /* --------------------------- Print to serial (for plotting) -------------------------- */
        
        
        timerUKF = 0;
    }
}

bool Main_bUpdateNonlinearX(Matrix &X_Next, Matrix &X, Matrix &U)
{
    /*  The update function in discrete time:
     *      x1(k+1) = x1(k) + x2(k)*dt
     *      x2(k+1) = x2(k) - g/l*sin(x1(k))*dt - alpha*x2*dt
     */
    float_prec theta     = X[0][0];
    float_prec theta_dot = X[1][0];
    
    if (theta > 3.14159265359) {
        theta = theta - 3.14159265359;
    }
    if (theta < -3.14159265359) {
        theta = theta + 3.14159265359;
    }
    
    X_Next[0][0] = theta + (theta_dot*SS_DT);   
    X_Next[1][0] = theta_dot + (-pend_g/pend_l*sin(theta) - pend_alpha*theta_dot)*SS_DT;
    
    return true;
}

bool Main_bUpdateNonlinearY(Matrix &Y, Matrix &X, Matrix &U)
{
    /*  The output (in discrete time):
     *      y1(k) =  sin(x1(k)) * l
     *      y2(k) = -cos(x1(k)) * l
     */
    float_prec theta     = X[0][0];
    
    Y[0][0] =  sin(theta) * pend_l;
    Y[1][0] = -cos(theta) * pend_l;
    
    return true;
}



