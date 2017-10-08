#include <ros/ros.h>
#include <ros/node_handle.h>
#include <diagnostic_updater/diagnostic_updater.h>
#include <diagnostic_updater/publisher.h>

#include <sensor_msgs/Imu.h>
#include <sensor_msgs/MagneticField.h>
#include <sensor_msgs/FluidPressure.h>
#include <geometry_msgs/Vector3Stamped.h>
#include <geometry_msgs/QuaternionStamped.h>

#include <imu_3dm_gx4/FilterOutput.h>
#include "imu.hpp"

using namespace imu_3dm_gx4;

#define kEarthGravity (9.80665)
#define PI (3.141592653)

ros::Publisher pubIMU;
ros::Publisher pubMag;
ros::Publisher pubPressure;
ros::Publisher pubFilter;
std::string frameId;

Imu::Info info;
Imu::DiagnosticFields fields;

//  diagnostic_updater resources
std::shared_ptr<diagnostic_updater::Updater> updater;
std::shared_ptr<diagnostic_updater::TopicDiagnostic> imuDiag;
std::shared_ptr<diagnostic_updater::TopicDiagnostic> filterDiag;

void publishData(const Imu::IMUData &data) {
  sensor_msgs::Imu imu;
  sensor_msgs::MagneticField field;
  sensor_msgs::FluidPressure pressure;

  //  assume we have all of these since they were requested
  /// @todo: Replace this with a mode graceful failure...
  assert(data.fields & Imu::IMUData::Accelerometer);
  assert(data.fields & Imu::IMUData::Magnetometer);
  assert(data.fields & Imu::IMUData::Barometer);
  assert(data.fields & Imu::IMUData::Gyroscope);

  //  timestamp identically
  imu.header.stamp = ros::Time::now();
  imu.header.frame_id = frameId;
  field.header.stamp = imu.header.stamp;
  field.header.frame_id = frameId;
  pressure.header.stamp = imu.header.stamp;
  pressure.header.frame_id = frameId;

  imu.orientation_covariance[0] =
      -1; //  orientation data is on a separate topic

  imu.linear_acceleration.x = data.accel[0] * kEarthGravity;
  imu.linear_acceleration.y = data.accel[1] * kEarthGravity;
  imu.linear_acceleration.z = data.accel[2] * kEarthGravity;
  imu.angular_velocity.x = data.gyro[0];
  imu.angular_velocity.y = data.gyro[1];
  imu.angular_velocity.z = data.gyro[2];

  field.magnetic_field.x = data.mag[0];
  field.magnetic_field.y = data.mag[1];
  field.magnetic_field.z = data.mag[2];

  pressure.fluid_pressure = data.pressure;

  //  publish
  pubIMU.publish(imu);
  pubMag.publish(field);
  pubPressure.publish(pressure);
  if (imuDiag) {
    imuDiag->tick(imu.header.stamp);
  }
}

void publishFilter(const Imu::FilterData &data) {
  assert(data.fields & Imu::FilterData::Quaternion);
  assert(data.fields & Imu::FilterData::OrientationEuler);
  assert(data.fields & Imu::FilterData::Acceleration);
  assert(data.fields & Imu::FilterData::AngularRate);
  assert(data.fields & Imu::FilterData::Bias);
  assert(data.fields & Imu::FilterData::AngleUnertainty);
  assert(data.fields & Imu::FilterData::BiasUncertainty);

  imu_3dm_gx4::FilterOutput output;
  output.header.stamp = ros::Time::now();
  output.header.frame_id = frameId;
  output.quaternion.w = data.quaternion[0];
  output.quaternion.x = data.quaternion[1];
  output.quaternion.y = data.quaternion[2];
  output.quaternion.z = data.quaternion[3];
  output.quaternion_status = data.quaternionStatus;

  output.euler_rpy.x = data.eulerRPY[0];
  output.euler_rpy.y = data.eulerRPY[1];
  output.euler_rpy.z = data.eulerRPY[2];
  output.euler_rpy_status = data.eulerRPYStatus;

  output.euler_angle_covariance[0] = data.eulerAngleUncertainty[0]*
      data.eulerAngleUncertainty[0];
  output.euler_angle_covariance[4] = data.eulerAngleUncertainty[1]*
      data.eulerAngleUncertainty[1];
  output.euler_angle_covariance[8] = data.eulerAngleUncertainty[2]*
      data.eulerAngleUncertainty[2];
  output.euler_angle_covariance_status = data.eulerAngleUncertaintyStatus;

  output.gyro_bias.x = data.gyroBias[0];
  output.gyro_bias.y = data.gyroBias[1];
  output.gyro_bias.z = data.gyroBias[2];
  output.gyro_bias_status = data.gyroBiasStatus;

  output.gyro_bias_covariance[0] = data.gyroBiasUncertainty[0]*data.gyroBiasUncertainty[0];
  output.gyro_bias_covariance[4] = data.gyroBiasUncertainty[1]*data.gyroBiasUncertainty[1];
  output.gyro_bias_covariance[8] = data.gyroBiasUncertainty[2]*data.gyroBiasUncertainty[2];
  output.gyro_bias_covariance_status = data.gyroBiasUncertaintyStatus;

  output.linear_acceleration.x = data.acceleration[0];
  output.linear_acceleration.y = data.acceleration[1];
  output.linear_acceleration.z = data.acceleration[2];
  output.linear_acceleration_status = data.accelerationStatus;

  output.angular_velocity.x = data.angularRate[0];
  output.angular_velocity.y = data.angularRate[1];
  output.angular_velocity.z = data.angularRate[2];
  output.angular_velocity_status = data.angularRateStatus;

  pubFilter.publish(output);
  if (filterDiag) {
    filterDiag->tick(output.header.stamp);
  }
}

std::shared_ptr<diagnostic_updater::TopicDiagnostic> configTopicDiagnostic(
    const std::string& name, double * target) {
  std::shared_ptr<diagnostic_updater::TopicDiagnostic> diag;
  const double period = 1.0 / *target;  //  for 1000Hz, period is 1e-3

  diagnostic_updater::FrequencyStatusParam freqParam(target, target, 0.01, 10);
  diagnostic_updater::TimeStampStatusParam timeParam(0, period * 0.5);
  diag.reset(new diagnostic_updater::TopicDiagnostic(name,
                                                     *updater,
                                                     freqParam,
                                                     timeParam));
  return diag;
}

void updateDiagnosticInfo(diagnostic_updater::DiagnosticStatusWrapper& stat,
                          imu_3dm_gx4::Imu* imu) {
  //  add base device info
  std::map<std::string,std::string> map = info.toMap();
  for (const std::pair<std::string,std::string>& p : map) {
    stat.add(p.first, p.second);
  }

  try {
    //  try to read diagnostic info
    imu->getDiagnosticInfo(fields);

    auto map = fields.toMap();
    for (const std::pair<std::string, unsigned int>& p : map) {
      stat.add(p.first, p.second);
    }
    stat.summary(diagnostic_msgs::DiagnosticStatus::OK, "Read diagnostic info.");
  }
  catch (std::exception& e) {
    const std::string message = std::string("Failed: ") + e.what();
    stat.summary(diagnostic_msgs::DiagnosticStatus::ERROR, message);
  }
}

int main(int argc, char **argv) {
  ros::init(argc, argv, "imu_3dm_gx4");
  ros::NodeHandle nh("~");

  std::string device;
  int baudrate;
  bool enableFilter;
  bool enableMagUpdate, enableAccelUpdate;
  int requestedImuRate, requestedFilterRate;
  bool verbose;

  //Variables for IMU reference position
  std::string desiredHeadingUpdateSource, desiredDeclinationSource;
  std::string headingUpdateSource, declinationSource;
  std::string location;
  float desiredRoll, desiredPitch, desiredYaw;
  float roll, pitch, yaw;
  double desiredLatitude, desiredLongitude, desiredAltitude, manualDeclination;
  double latitude, longitude, altitude, declination;
  bool enable_sensor_to_vehicle_tf;

  //  load parameters from launch file
  nh.param<std::string>("device", device, "/dev/ttyACM0");
  nh.param<int>("baudrate", baudrate, 115200);
  nh.param<std::string>("frame_id", frameId, std::string("imu"));
  nh.param<int>("imu_rate", requestedImuRate, 100);
  nh.param<int>("filter_rate", requestedFilterRate, 100);
  nh.param<bool>("enable_filter", enableFilter, true);
  nh.param<bool>("enable_mag_update", enableMagUpdate, true);
  nh.param<bool>("enable_accel_update", enableAccelUpdate, true);
  nh.param<bool>("verbose", verbose, false);

  //Additional parameters - used to set IMU reference position
  nh.param<std::string>("location", location, (std::string)"columbus");
  nh.param<double>("latitude", desiredLatitude, 39.9984f); //Default is Columbus latitude
  nh.param<double>("longitude", desiredLongitude, -83.0179f); //Default is Columbus longitude
  nh.param<double>("altitude", desiredAltitude, 224.0f); //Default is Columbus altitude
  nh.param<double>("declination", manualDeclination, 7.01f); //Default is Columbus declination
  nh.param<float>("roll", desiredRoll, 0.0f); //Default is 0.0
  nh.param<float>("pitch", desiredPitch, -90.0f); //Default is -90.0
  nh.param<float>("yaw", desiredYaw, 180.0f); //Default is 180.0
  nh.param<bool>("enable_sensor_to_vehicle_tf", enable_sensor_to_vehicle_tf, true); //Default is Columbus declination
  nh.param<std::string>("heading_update_source", desiredHeadingUpdateSource, std::string("magnetometer")); //Default is magnetometer
  nh.param<std::string>("declination_source", desiredDeclinationSource, std::string("wmm")); //Default is World Magnetic Model

  if (requestedFilterRate < 0 || requestedImuRate < 0) {
    ROS_ERROR("imu_rate and filter_rate must be > 0");
    return -1;
  }

  pubIMU = nh.advertise<sensor_msgs::Imu>("imu", 1);
  pubMag = nh.advertise<sensor_msgs::MagneticField>("magnetic_field", 1);
  pubPressure = nh.advertise<sensor_msgs::FluidPressure>("pressure", 1);

  if (enableFilter) {
    pubFilter = nh.advertise<imu_3dm_gx4::FilterOutput>("filter", 1);
  }

  //  new instance of the IMU
  Imu imu(device, verbose);
  try {
    imu.connect();

    ROS_INFO("Selecting baud rate %u", baudrate);
    imu.selectBaudRate(baudrate);

    ROS_INFO("Fetching device info.");
    imu.getDeviceInfo(info);
    std::map<std::string,std::string> map = info.toMap();
    for (const std::pair<std::string,std::string>& p : map) {
      ROS_INFO("\t%s: %s", p.first.c_str(), p.second.c_str());
    }

    ROS_INFO("Idling the device");
    imu.idle();

    //  read back data rates
    uint16_t imuBaseRate, filterBaseRate;
    imu.getIMUDataBaseRate(imuBaseRate);
    ROS_INFO("IMU data base rate: %u Hz", imuBaseRate);
    imu.getFilterDataBaseRate(filterBaseRate);
    ROS_INFO("Filter data base rate: %u Hz", filterBaseRate);

    //  calculate decimation rates
    if (static_cast<uint16_t>(requestedImuRate) > imuBaseRate) {
      throw std::runtime_error("imu_rate cannot exceed " +
                               std::to_string(imuBaseRate));
    }
    if (static_cast<uint16_t>(requestedFilterRate) > filterBaseRate) {
      throw std::runtime_error("filter_rate cannot exceed " +
                               std::to_string(filterBaseRate));
    }

    const uint16_t imuDecimation = imuBaseRate / requestedImuRate;
    const uint16_t filterDecimation = filterBaseRate / requestedFilterRate;

    ROS_INFO("Selecting IMU decimation: %u", imuDecimation);
    imu.setIMUDataRate(
        imuDecimation, Imu::IMUData::Accelerometer |
          Imu::IMUData::Gyroscope |
          Imu::IMUData::Magnetometer |
          Imu::IMUData::Barometer);

    ROS_INFO("Selecting filter decimation: %u", filterDecimation);
    imu.setFilterDataRate(filterDecimation, Imu::FilterData::Quaternion |
                          Imu::FilterData::OrientationEuler |
                          Imu::FilterData::Acceleration |
                          Imu::FilterData::AngularRate |
                          Imu::FilterData::Bias |
                          Imu::FilterData::AngleUnertainty |
                          Imu::FilterData::BiasUncertainty);

    ROS_INFO("Enabling IMU data stream");
    imu.enableIMUStream(true);

    if (enableFilter) {
      ROS_INFO("Enabling filter data stream");
      imu.enableFilterStream(true);

      ROS_INFO("Enabling filter measurements");
      imu.enableMeasurements(enableAccelUpdate, enableMagUpdate);

      ROS_INFO("Enabling gyro bias estimation");
      imu.enableBiasEstimation(true);
    } else {
      ROS_INFO("Disabling filter data stream");
      imu.enableFilterStream(false);
    }
    imu.setIMUDataCallback(publishData);
    imu.setFilterDataCallback(publishFilter);

    // Set IMU Reference Position Settings///////////////////////////////
    //Convert to radians
    desiredRoll *= (PI/180);
    desiredPitch *= (PI/180);
    desiredYaw *= (PI/180);
    manualDeclination *= (PI/180);

    ROS_INFO("Location = %s", location.c_str());
    ROS_INFO("Setting Sensor to Vehicle Frame Transformation");
    imu.setSensorToVehicleTF(desiredRoll, desiredPitch, desiredYaw);
    imu.getSensorToVehicleTF(roll, pitch, yaw);
    roll *= (180/PI); //convert to degrees
    pitch *= (180/PI); //convert to degrees
    yaw *= (180/PI); //convert to degrees
    ROS_INFO("\tDesired Roll: %f", desiredRoll*180/PI);
    ROS_INFO("\tDesired Pitch: %f", desiredPitch*180/PI);
    ROS_INFO("\tDesired Yaw: %f", desiredYaw*180/PI);
    ROS_INFO("\tCurrent Roll: %f", roll);
    ROS_INFO("\tCurrent Pitch: %f", pitch);
    ROS_INFO("\tCurrent Yaw: %f", yaw);

    ROS_INFO("Setting Heading Update Source");
    imu.setHeadingUpdateSource(desiredHeadingUpdateSource);
    imu.getHeadingUpdateSource(headingUpdateSource);
    ROS_INFO("\tDesired Source: %s", desiredHeadingUpdateSource.c_str());
    ROS_INFO("\tCurrent Source: %s", headingUpdateSource.c_str());

    ROS_INFO("Setting Reference Position");
    imu.setReferencePosition(desiredLatitude, desiredLongitude, desiredAltitude);
    imu.getReferencePosition(latitude, longitude, altitude);
    ROS_INFO("\tDesired Latitude: %f", desiredLatitude);
    ROS_INFO("\tDesired Longitude: %f", desiredLongitude);
    ROS_INFO("\tDesired Altitude: %f", desiredAltitude);
    ROS_INFO("\tCurrent Latitude: %f", latitude);
    ROS_INFO("\tCurrent Longitude: %f", longitude);
    ROS_INFO("\tCurrent Altitude: %f", altitude);

    ROS_INFO("Setting Declination Source");
    imu.setDeclinationSource(desiredDeclinationSource, manualDeclination);
    imu.getDeclinationSource(declinationSource, declination);
    declination *= (180/PI);
    ROS_INFO("\tDesired Source: %s", desiredDeclinationSource.c_str());
    ROS_INFO("\tCurrent Source: %s", declinationSource.c_str());
    ROS_INFO("\tManual Declination: %f", manualDeclination*180/PI);
    ROS_INFO("\tCurrent Declination: %f", declination);
    //////////////////////////////////////////////////////////////////////

    //  configure diagnostic updater
    if (!nh.hasParam("diagnostic_period")) {
      nh.setParam("diagnostic_period", 0.2);  //  5hz period
    }

    updater.reset(new diagnostic_updater::Updater());
    const std::string hwId = info.modelName + "-" + info.modelNumber;
    updater->setHardwareID(hwId);

    //  calculate the actual rates we will get
    double imuRate = imuBaseRate / (1.0 * imuDecimation);
    double filterRate = filterBaseRate / (1.0 * filterDecimation);
    imuDiag = configTopicDiagnostic("imu",&imuRate);
    if (enableFilter) {
      filterDiag = configTopicDiagnostic("filter",&filterRate);
    }

    updater->add("diagnostic_info",
                 boost::bind(&updateDiagnosticInfo, _1, &imu));

    ROS_INFO("Resuming the device");
    imu.resume();

    while (ros::ok()) {
      imu.runOnce();
      updater->update();
    }
    imu.disconnect();
  }
  catch (Imu::io_error &e) {
    ROS_ERROR("IO error: %s\n", e.what());
  }
  catch (Imu::timeout_error &e) {
    ROS_ERROR("Timeout: %s\n", e.what());
  }
  catch (std::exception &e) {
    ROS_ERROR("Exception: %s\n", e.what());
  }

  return 0;
}
