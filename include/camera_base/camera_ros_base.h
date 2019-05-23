#ifndef CAMERA_ROS_BASE_H_
#define CAMERA_ROS_BASE_H_

#include <ros/ros.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/CameraInfo.h>
#include <sensor_msgs/image_encodings.h>
#include <image_transport/image_transport.h>
#include <camera_info_manager/camera_info_manager.h>
#include <diagnostic_updater/publisher.h>
#include <diagnostic_updater/diagnostic_updater.h>
#include <memory>

namespace camera_base {

/**
 * @brief getParam Util function for getting ros parameters under nodehandle
 * @param nh Node handle
 * @param name Parameter name
 * @return Parameter value
 */
template <typename T>
T getParam(const ros::NodeHandle& nh, const std::string& name) {
  T value{};
  if (!nh.getParam(name, value)) {
    ROS_ERROR("Cannot find parameter: %s", name.c_str());
  }
  return value;
}

/**
 * @brief The CameraRosBase class
 * This class implements a ros camera
 */
class CameraRosBase {
 public:
  explicit CameraRosBase(const ros::NodeHandle& pnh,
                         const std::string& prefix = std::string())
      : pnh_(pnh),
        cnh_(pnh, prefix),
        it_(cnh_),
        camera_pub_(it_.advertiseCamera("image_raw", 1)),
        cinfo_mgr_(cnh_, getParam<std::string>(cnh_, "camera_name"),
                   getParam<std::string>(cnh_, "calib_url")),
        fps_(10.0),
        diagnostic_updater_(pnh_, cnh_) {
    SetTopicDiagnosticParameters(fps_ * 0.9, fps_ * 1.1, 10.0, -0.01, 0.1);
    cnh_.param<std::string>("frame_id", frame_id_, cnh_.getNamespace());
    cnh_.param<std::string>("identifier", identifier_, "");
  }
  /**
   * @brief SetTopicDiagnosticParameters Set limits for topic diagnostics
   * @param minFreq  min allowed frequency
   * @param maxFreq  max allowed frequency
   * @param windowSize [sec]
   * @param minDelay  min allowed delay of timestamp [how much early]
   * @param maxDelay  max allowed delay of timestamp [how much late]
   */
  void SetTopicDiagnosticParameters(double minFreq, double maxFreq,
                                    double windowSize,
                                    double minDelay, double maxDelay) {
    min_fps_ = minFreq;
    max_fps_ = maxFreq;
    std::string prefix = cnh_.getNamespace();
    std::string name = prefix.empty() ? "image_raw" : (prefix + "/image_raw");

    // must first remove old updater
    diagnostic_updater_.removeByName(name + " topic status");
    topic_diagnostic_.reset(new diagnostic_updater::TopicDiagnostic(
      name, diagnostic_updater_,
      diagnostic_updater::FrequencyStatusParam(&min_fps_, &max_fps_, 0.0, windowSize),
        diagnostic_updater::TimeStampStatusParam(minDelay, maxDelay)));

  }


  CameraRosBase() = delete;
  CameraRosBase(const CameraRosBase&) = delete;
  CameraRosBase& operator=(const CameraRosBase&) = delete;
  virtual ~CameraRosBase() = default;

  const std::string& identifier() const { return identifier_; }
  const std::string& frame_id() const { return frame_id_; }

  double fps() const { return fps_; }
  void set_fps(double fps) { fps_ = fps; }

  /**
   * @brief SetHardwareId Set hardware id for diagnostic updater
   * @param id harware id
   */
  void SetHardwareId(const std::string& id) {
    diagnostic_updater_.setHardwareID(id);
  }
  /**
   * @brief PublishCamera Publish a camera topic with Image and CameraInfo
   * @param time Acquisition time stamp
   */
  void PublishCamera(const ros::Time& time) {
    const auto image_msg = boost::make_shared<sensor_msgs::Image>();
    const auto cinfo_msg =
        boost::make_shared<sensor_msgs::CameraInfo>(cinfo_mgr_.getCameraInfo());
    image_msg->header.frame_id = frame_id_;
    image_msg->header.stamp = time;
    if (Grab(image_msg, cinfo_msg)) {
      // Update camera info header
      cinfo_msg->header = image_msg->header;
      camera_pub_.publish(image_msg, cinfo_msg);
      topic_diagnostic_->tick(image_msg->header.stamp);
    }
    diagnostic_updater_.update();
  }

  void Publish(const sensor_msgs::ImagePtr& image_msg) {
    const auto cinfo_msg =
        boost::make_shared<sensor_msgs::CameraInfo>(cinfo_mgr_.getCameraInfo());
    // Update camera info header
    image_msg->header.frame_id = frame_id_;
    cinfo_msg->header = image_msg->header;
    camera_pub_.publish(image_msg, cinfo_msg);
    topic_diagnostic_->tick(image_msg->header.stamp);
    diagnostic_updater_.update();
  }

  /**
   * @brief Grab Fill image_msg and cinfo_msg from low level camera driver
   * @param image_msg Ros message ImagePtr
   * @return True if successful
   */
  virtual bool Grab(const sensor_msgs::ImagePtr& image_msg,
                    const sensor_msgs::CameraInfoPtr& cinfo_msgs = nullptr) = 0;

  int getNumSubscribers() const {
    return (camera_pub_.getNumSubscribers());
  }

 private:
  ros::NodeHandle pnh_;
  ros::NodeHandle cnh_;
  image_transport::ImageTransport it_;
  image_transport::CameraPublisher camera_pub_;
  camera_info_manager::CameraInfoManager cinfo_mgr_;
  double fps_;
  double min_fps_;
  double max_fps_;
  diagnostic_updater::Updater diagnostic_updater_;
  std::shared_ptr<diagnostic_updater::TopicDiagnostic> topic_diagnostic_;
  std::string frame_id_;
  std::string identifier_;
};

}  // namespace camera_base

#endif  // ROS_CAMERA_BASE_H_
