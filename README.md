The imu_3dm_gx4 Package
=======================

![Picture of IMU](https://www.microstrain.com/sites/default/files/styles/larger__550x550_/public/gx4-25.jpg?itok=vB8GWQpI)

The `imu_3dm_gx4` package provides support for the [Lord Corporation](http://www.microstrain.com) Microstrain [3DM-GX4-25](http://www.microstrain.com/inertial/3dm-gx4-25) series IMU. The package employs the MIP packet format, so it could conceivably be adapted to support other versions of Microstrain products with relatively little effort. At present, the 15 and 45 series AHRS systems are not supported.

Supported platforms: Ubuntu 12.04, 14.04, and 16.04

NOTE:
This package originates from [KumarRobotics/imu_3dm_gx4](https://github.com/KumarRobotics/imu_3dm_gx4)) and has been adapted extensively from the 0.0.4 version.

# Version History

## UWRT
* **0.1.5**
  - Placed imu node in its own namespace.
  - Created templated settings file for convenience.
* **0.1.4**
  - All fields referring to commands, reply fields, and data fields inside the `imu.cpp` file have been renamed to match those appearing in the LORD Microstrain 3DM-GX4-25 Data and Communications Protocol manual.
  - Removes `enable_filter` argument so the Adaptive Extended Kalman Filter (AEKF) output is always present.
  - Updated `/imu/magnetic_field` message type from `sensor_msgs/MagneticField` to `imu_3dm_gx4/MagFieldCF`.
  - Added more data fields provided by AEKF to `imu_3dm_gx4/FilterOutput` message.
  - Added launch file arguments for: reference location, heading update source, declination source, sensor LPF bandwidths, and enabling iron offsets loaded from file
  
## KumarRobotics
* **0.0.4**:
  - Fixed issue where packets would be dropped if the header checksum was broken up
  into multiple packets.
  - Added `verbose` option.
* **0.0.3**:
  - Replaced `decimation` options with `rate` options. Decimation is automatically calculated.
  - Added a file for use with Kalibr.
  - Added a udev rule to configure permissions.
* **0.0.2**:
  - Units of acceleration are now m/s^2, in agreement with ROS convention.
  - Cleaned up code base, replaced error codes with exceptions.
  - Status can now be viewed from `rqt_runtime_monitor`.
  - Filter output is now in a single, custom message with covariances and important status flags.
  - Added option to enable/disable accelerometer update in the estimator.
  - Removed TF broadcast option.
  - Reformatted code base to clang-llvm convention.
* **0.0.1**:
  - First release.

# Primary Files

## Source Code (src/)
* imu.hpp
  - This is the header file for the `imu_3dm_gx4` driver.
* imu.cpp
  - This is the cpp file for `imu_3dm_gx4` driver.
  - All COMMANDS, REPLY_FIELDS, and DATA fields have been renamed for convenience.
* imu_3dm_gx4.cpp
  - This file creates the ROS node that interfaces with the imu.cpp file.

## Messages (msg/)
* FilterOutput
  - This file contains output from the IMU's AEKF. This message has been revised from the original KumarRobotics message to include additional data fields. This message contains:
    * Orientation estimates in Euler Angles (rad) and the covariances (rad^2), and status flags
    * Orientation estimates in Quaternions and status flags
    * Heading updates (see "Set Heading Update for more info")
      - `heading_update_LORD`: Returns the heading update performed by LORD Microstrains AEKF (rad)
      - `heading_update_alt`: Returns the heading update performed by this driver (rad)
    * Filtered linear accelerations (m/s^2) and angular velocities (rad/s), and status flags
    * Gyro bias and covariances (rad^2)
* MagFieldCF
  - This file contains magnetometer data provided by the complimentary filter. This message contains:
    * The magnetic field components (Gauss)
    * Total magnetic field magnitude (Gauss)
    * Covariance of the magnetic field (Gauss^2)

# How to Launch the IMU
You can use the launch file included in this package, or you may use it as a template and place your modified launch file somewhere in your catkin workspace. To run the launch file included in this package:
```
roslaunch imu_3dm_gx4 imu.launch
```
There are a few parameters that can be set as arguments in the launch file: `imu_name`, `device`, and `frame_id`

Note: You may need to change the device path for your specific IMU (default is `/dev/ttyACM0`). You can change the `device` argument in your own launch file, or you may pass it via command-line:
```
roslaunch imu_3dm_gx4 imu.launch device:=/dev/ttyACM1
```

## ROS Topics

On launch, the node will configure the IMU according to the parameters and then enable streaming node. All topics are placed into the namespace according to the `imu_name` parameter in the launch file, should you need to launch multiple IMUs. The following topics are published with synchronized timestamps:

* `/<imu_name>/imu`: An instance of `sensor_msgs/Imu`. Orientation quaternion not provided in this message since is already part of the filter message.
* `/<imu_name>/magnetic_field`: An instance of `imu_3dm_gx4/MagFieldCF`.
* `/<imu_name>/pressure`: An instance of `sensor_msgs/FluidPressure`.

The topic for the IMU's estimation filter is published on a separate topic on an asyhcnronous timestamp:

* `/<imu_name>/filter`: An instance of `imu_3dm_gx4/FilterOuput`.

# Settings 
The following settings are organized according to [LORD Microstrain's 3DM-GX4-25 Data Communications Protocol manual](http://files.microstrain.com/3DM-GX4-25%20Data%20Communications%20Protocol.pdf).

This package comes with a templated settings YAML file: `cfg/default_settings.yaml`. In your own launch file, you may pass in such a file path and this driver will use those parameters instead of the default ones.

## 3DM Settings
### High-Level Settings
The `imu_3dm_gx4` node supports the following base settings:
* `device` (Default is `/dev/ttyACM0`): Path to the device in `/dev/...`
* `baudrate` (Defaults is `115200`): Baudrate to employ with serial communication.
* `frame_id`: Frame to use in headers.
* `imu_rate` (Default is `100`): Controls the rate at which ALL THREE of the IMU's sensors output data (synchronously), in Hz.
* `filter_rate` (Default is `100`): Controls the rate at which the estimation filter (the AEKF, not the Complimentary Filter) is ran, in Hz.
  - Note: Output from the AEKF is NOT synchronous, unlike the IMU sensor data, and time-steps between outputs will be non-constant (but are close enough to what the rate is set to).
* `verbose`: If true, packet reads and mismatched checksums will be logged.

#### Note on Rate Parameters:

The `_rate` options command the requested frequency by calculating a 'decimation value', which is defined by the relation:

```
  frequency = base_frequency / decimation
```

Where the base frequency is 1kHz for the GX4. Since decimation values are integers, certain frequencies cannot be expressed by the above relation. 800Hz, for example, cannot be specified by an integer decimation rate. In these cases, you will receive whichever frequency is obtained by rounding decimation down to the nearest integer. Hence, requesting 800Hz will produce 1kHz.

### Set Low Pass Filter Bandwidths
The following options allow the user to change the IMU's internal infinite impulse response (IIR) low pass filter (LPF) bandwidths on each of the IMU's sensors. A value of `-1` will disable the internal IIR LPF.
* `mag_LPF_Bandwidth` (Default is `-1`): LPF bandwidth for the magnetometer, in Hz
* `accel_LPF_Bandwidth` (Default is `-1`): LPF bandwidth for the accelerometer, in Hz
* `gyro_LPF_Bandwidth` (Default is `-1`): LPF bandwidth for the gyroscope, in Hz


### Pre-load Hard and Soft Iron Coefficients
The following option is for setting the IMU to load a pre-defined set of Hard and Soft Iron coefficients:
* `enable_iron_offset` (Default is `false`); Indicates if IMU should load pre-defined coefficients from a YAML file. 
  - The default hard iron offsets along the x, y, and z axes are all set to 0.
  - The default soft-iron matrix is set to the 3x3 identity matrix.
  - Please see the `cfg/default_settings.yaml` file for the specific format of loading the hard and soft-iron parameters.

## Estimation Filter Settings
### Sensor-to-Vehicle Transformation
The sensor-to-vehicle transformation feature allows the user to specify the coordinate transformation FROM the sensor frame TO the vehicle frame (via Euler Angles in the order of yaw, pitch, then roll). Default values are assumed to be `0 deg`.
* `roll`: Roll angle, in deg.
* `pitch`: Pitch angle, in deg.
* `yaw`: Yaw angle, in deg.

### Set Reference Location and Heading Update
The following options are required for the heading update feature:
* `latitude` (Default is `39.9984`): Latitude, in deg.
* `longitude` (Default is `-83.0179`): Longitude, in deg.
* `altitude` (Default is `224.0`): Altitude, in meters
* `declination` (Default is `-7.110`): Declination angle, in deg.
* `heading_update_source` (Default is `magnetometer`): Possible options are: `none`, `external`, or `magnetometer`
  - IMPORTANT: The 3DM-GX4-25 model does NOT perform the heading update well in the presence of EMI. The manufacturer claims setting the IIR LPF bandwidth on the magnetometer affects the values used by the AEKF in this correction step. Through trial and error, setting the magnetometer bandwidth below that of the EMI does improve the heading update output, however, the output still exhibits considerable sinusoidal behavior (unusable for yaw control).
    * This heading update appears as `heading_update_LORD` within the `imu_3dm_gx4/FilterOutput` message.
  - This driver offers an ALTERNATIVE heading update feature which performs the calculation correctly.
    * This alternative heading update appears as `heading_update_alt` within the `imu_3dm_gx4/FilterOutput` message.
* `declination_source` (Default is `wmm`): Possible options are: `none`, `wmm`, or `manual`
  - Note: `wmm` indicates the IMU should use its internal World Magnetometer Model (the GX4 has the 2005 model preloaded)

## Known Issues

* Even when the `enable_mag_update` option is set to false (and the device acknowledges the setting with a positive ACK), the `quat_status` field is received as 3. This has not been fully debugged yet.

## Specifications

* Specifications can be found at [3DM-GX4-25](http://www.microstrain.com/inertial/3dm-gx4-25).

We provide YAML file to work with [Kalibr](https://github.com/ethz-asl/kalibr), which can be found in the `calib` folder.

## FAQs

1. What data rates can I use?
The driver supports up to 1000Hz for IMU data, and up to 500Hz for filter data. For high data rates, it is recommended that you use a baudrate of 921600.

2. The driver can't open my device, even though the path is specified correctly - what gives??
Make sure you have ownership of the device in `/dev`, or are part of the dialout group.
