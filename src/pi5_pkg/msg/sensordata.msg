# sensordata.msg
#takes inputs from rear sonar, front and rear cliff, LD2450 radar, MPU6050 and VL53L5X

#rear sonar dist = 0 or 400 means invalid. 
int rear_sonar_dist

#for cliff sensors
bool front_cliff_detected
bool rear_cliff_detected

#for LD2450 radar
#float32[] distances

#for VL53L5x

#for MPU6050

#for raw wheel speeds

#for vehicle speed and heading angle
