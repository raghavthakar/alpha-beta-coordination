#include "ros/ros.h"
#include "geometry_msgs/Pose.h"
#include "geometry_msgs/Twist.h"
#include "nav_msgs/Odometry.h"
#include "tf/transform_datatypes.h"
#include "tf/LinearMath/Matrix3x3.h"
#include "std_msgs/Float64.h"
#include <string>
#include <cmath>
#include <actionlib/server/simple_action_server.h>
#include <abc_control/MoveDistanceAction.h>
#include <abc_control/MoveDistanceGoal.h>
#include <abc_control/MoveDistanceResult.h>
#include <abc_control/MoveDistanceFeedback.h>

// linear velocity of the robot
#define BASE_LINEAR_VEL 0.15

const double KP=0.5;
const double DT=0.15;

class Mover
{
    ros::NodeHandle node_handle;
    actionlib::SimpleActionServer<abc_control::MoveDistanceAction> server;
    nav_msgs::OdometryConstPtr current_odom;
    std::string odom_topic;
    std::string twist_topic;
    geometry_msgs::Twist twist_message;
    ros::Publisher twist_publisher;

    std::vector<double> getOrientation()
    {
        double roll, pitch, yaw;
        std::vector<double> angles;

        //Get the current odom data of the robot
        current_odom=ros::topic::waitForMessage<nav_msgs::Odometry>(odom_topic);

        // the incoming geometry_msgs::Quaternion is transformed to a tf::Quaterion
        tf::Quaternion quat;
        tf::quaternionMsgToTF(current_odom->pose.pose.orientation, quat);

        // the tf::Quaternion has a method to acess roll pitch and yaw
        tf::Matrix3x3(quat).getRPY(roll, pitch, yaw);

        angles.push_back(roll);
        angles.push_back(pitch);
        angles.push_back(yaw);

        return angles;
    }

    double getCurrentYaw()
    {
        double roll, pitch, yaw;

        //Get the current odom data of the robot
        current_odom=ros::topic::waitForMessage<nav_msgs::Odometry>(odom_topic);

        // the incoming geometry_msgs::Quaternion is transformed to a tf::Quaterion
        tf::Quaternion quat;
        tf::quaternionMsgToTF(current_odom->pose.pose.orientation, quat);

        // the tf::Quaternion has a method to acess roll pitch and yaw
        tf::Matrix3x3(quat).getRPY(roll, pitch, yaw);

        return yaw;
    }

    double distanceFrom(double target_x, double target_y)
    {
        return sqrt(pow(target_y-current_odom->pose.pose.position.y, 2)
                +pow(target_x-current_odom->pose.pose.position.x, 2));
    }

    double getAngleToTarget(double target_y, double current_y,
                          double target_x, double current_x)
    {
        //Find the angle to rotate to face target
        double angle_to_target = atan((target_y-current_y)/
                                      (target_x-current_x));

        if(target_x-current_x<=0)
          if(target_y-current_y>=0)
            angle_to_target+=3.14;
          else
            angle_to_target-=3.14;

        if(angle_to_target>3.14)
            angle_to_target-=3.14;
        else if(angle_to_target<-3.14)
            angle_to_target+=3.14;

        ROS_INFO("Returning");
        return angle_to_target;
    }

    void move(const abc_control::MoveDistanceGoalConstPtr& goal)
    {
        ROS_INFO_STREAM("In the callback");
        for(int i=0; i<100; i++)
        {
            ROS_INFO_STREAM("In the loop");
            ROS_INFO_STREAM(odom_topic);
            current_odom=ros::topic::waitForMessage<nav_msgs::Odometry>(odom_topic);

            double angle_to_target=getAngleToTarget(goal->target.y, current_odom->
                                          pose.pose.position.y, goal->target.x,
                                          current_odom->pose.pose.position.x);

            ROS_INFO("TARGET X: %f Y: %f", goal->target.x, goal->target.y);

            //linear velocity will always be this
            twist_message.linear.x=BASE_LINEAR_VEL;

            //Move the robot forwards till we reach the target
            while(fabs(distanceFrom(goal->target.x, goal->target.y))>0.1)
            {
                //Get the current odom data of the robot
                current_odom=ros::topic::waitForMessage<nav_msgs::Odometry>(odom_topic);

                double yaw_error= getAngleToTarget(goal->target.y, current_odom->
                                              pose.pose.position.y, goal->target.x,
                                              current_odom->pose.pose.position.x) - getCurrentYaw();

                // For angular controller
                double p_effort=yaw_error*KP;

                // Set the angular yaw
                twist_message.angular.z=p_effort;

                twist_publisher.publish(twist_message);
                ROS_INFO("DISTANCE TO TARGET: %f", distanceFrom(goal->target.x, goal->target.y));
                ROS_INFO("YAW ERROR: %f", yaw_error);
                ROS_INFO("P EFFORT: %f", p_effort);
            }

            //Stop the robot
            twist_message.angular.z=0;
            twist_message.linear.x=0;
            twist_publisher.publish(twist_message);
        }
    }

public:
    // consructor with intitializer list
    Mover(char** argv):server(node_handle, "move_distance", boost::bind(&Mover::move, this, _1), false)
    {
        ROS_INFO_STREAM("in the constructor");
        // setting up the variables that will be used throughout
        odom_topic=odom_topic.append("/");
        odom_topic=odom_topic.append(argv[1]);
        odom_topic=odom_topic.append("/abc/odom");

        twist_topic=twist_topic.append("/");
        twist_topic=twist_topic.append(argv[1]);
        twist_topic=twist_topic.append("/abc/cmd_vel");

        twist_publisher = node_handle.advertise
                          <geometry_msgs::Twist>(twist_topic, 10);

        server.start();
    }
};

int main(int argc, char** argv)
{
    ros::init(argc, argv, "move_distance_server");
    Mover mover(argv);
    ros::spin();
    return 0;
}