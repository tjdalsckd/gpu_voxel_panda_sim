// this is for emacs file handling -*- mode: c++; indent-tabs-mode: nil -*-

// -- BEGIN LICENSE BLOCK ----------------------------------------------
// This file is part of the GPU Voxels Software Library.
//
// This program is free software licensed under the CDDL
// (COMMON DEVELOPMENT AND DISTRIBUTION LICENSE Version 1.0).
// You can find a copy of this license in LICENSE.txt in the top
// directory of the source code.
//
// © Copyright 2018 FZI Forschungszentrum Informatik, Karlsruhe, Germany
//
// -- END LICENSE BLOCK ------------------------------------------------

//----------------------------------------------------------------------
/*!\file
 *
 * \author  Andreas Hermann
 * \date    2018-01-07
 *
 */
//----------------------------------------------------------------------/*
#include "gvl_ompl_planner_helper.h"
#include <Eigen/Dense>
#include <signal.h>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <ros/ros.h>
#include <pcl_ros/point_cloud.h>
#include <pcl/point_types.h>
#include <std_msgs/String.h>
#include <geometry_msgs/PoseStamped.h>
#include <sstream>

#include <sensor_msgs/JointState.h>
#include <gpu_voxels/GpuVoxels.h>
#include <gpu_voxels/helpers/MetaPointCloud.h>
#include <gpu_voxels/robot/urdf_robot/urdf_robot.h>
//#include <gpu_voxels/logging/logging_gpu_voxels.h>
#include <thread>

#include <stdio.h>
#include <iostream>
#define IC_PERFORMANCE_MONITOR
#include <icl_core_config/Config.h>
#include <icl_core_performance_monitor/PerformanceMonitor.h>

namespace ob = ompl::base;
namespace og = ompl::geometric;
using namespace gpu_voxels;
namespace bfs = boost::filesystem;
#define PI 3.141592
#define D2R 3.141592/180.0
#define R2D 180.0/3.141592
Vector3ui map_dimensions(300,300,500);

double joint_states[7] = {0,0,0,0,0,0,0};
double move_joint_values[7] = {-PI/8,-0.785085,0.0,-2.3555949,0.0,1.570,0};

void rosjointStateCallback(const sensor_msgs::JointState::ConstPtr& msg)
{
  //std::cout << "Got JointStateMessage" << std::endl;
  gvl->clearMap("myRobotMap");

  for(size_t i = 0; i < msg->name.size(); i++)
  {
      myRobotJointValues[msg->name[i]] = msg->position[i];
      joint_states[i] = msg->position[i];
  }
   joint_states[6] =    joint_states[6]+PI/4;
  // update the robot joints:
  gvl->setRobotConfiguration("myUrdfRobot", myRobotJointValues);
  // insert the robot into the map:
  gvl->insertRobotIntoMap("myUrdfRobot", "myRobotMap",(eBVM_OCCUPIED));
}

void 
roscallback(const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& msg)
{
  static float x(0.0);
  //gvl->clearMap("myEnvironmentMap");
  
  std::vector<Vector3f> point_data;
  point_data.resize(msg->points.size());

  for (uint32_t i = 0; i < msg->points.size(); i++)
  {
    	point_data[i].x = msg->points[i].x;
    	point_data[i].y = msg->points[i].y;
    	point_data[i].z = msg->points[i].z;
  }

  //my_point_cloud.add(point_data);
  my_point_cloud.update(point_data);

  // transform new pointcloud to world coordinates
  my_point_cloud.transformSelf(&tf);
  
  new_data_received = true;

  ////LOGGING_INFO(Gpu_voxels, "DistanceROSDemo camera callback. PointCloud size: " << msg->points.size() << endl);
  
}



GvlOmplPlannerHelper::GvlOmplPlannerHelper(const ob::SpaceInformationPtr &si)
    : ob::StateValidityChecker(si)
    , ob::MotionValidator(si)
{




    si_ = si;
    stateSpace_ = si_->getStateSpace().get();

    assert(stateSpace_ != nullptr);

    gvl = gpu_voxels::GpuVoxels::getInstance();
    gvl->initialize(300, 300, 500, 0.01);

    // We add maps with objects, to collide them
    gvl->addMap(MT_PROBAB_VOXELMAP,"myRobotMap");
    gvl->addMap(MT_BITVECTOR_VOXELLIST,"myRobotMap2");
    //gvl->addMap(MT_BITVECTOR_OCTREE, "myEnvironmentMap");

    gvl->addMap(MT_BITVECTOR_VOXELLIST,"mySolutionMap");
    gvl->addMap(MT_PROBAB_VOXELMAP,"myQueryMap");

    gvl->addRobot("myUrdfRobot", "panda_coarse/panda_7link.urdf", true);
    //gvl->visualizeMap("myEnvironmentMap");

    PERF_MON_ENABLE("pose_check");
    PERF_MON_ENABLE("motion_check");
    PERF_MON_ENABLE("motion_check_lv");
}



GvlOmplPlannerHelper::~GvlOmplPlannerHelper()
{
    gvl.reset(); // Not even required, as we use smart pointers.
}

void GvlOmplPlannerHelper::moveObstacle()
{
   //// gvl->clearMap("myEnvironmentMap");
    static float x(0.0);

    // use this to animate a single moving box obstacle
    //gvl->insertBoxIntoMap(Vector3f(1.5, 1.2 ,0.0), Vector3f(1.6, 1.3 ,1.0), "myEnvironmentMap", eBVM_OCCUPIED, 2);
   // gvl->insertPointCloudFromFile("myEnvironmentMap", "hanyang_coarse/hanyang.binvox", true,
     //                                gpu_voxels::eBVM_OCCUPIED, true, gpu_voxels::Vector3f(1.2+x, 1.0, 0.5),0.5);

     //gvl->insertPointCloudFromFile("myEnvironmentMap", "table.binvox", true,
     //                                 gpu_voxels::eBVM_OCCUPIED, true, gpu_voxels::Vector3f(0.85, 0.5, 0.0),1);
    // gvl->insertPointCloudFromFile("myEnvironmentMap", "shelf2.binvox", true,
    //                                  gpu_voxels::eBVM_OCCUPIED, true, gpu_voxels::Vector3f(1.55, 0.8, 0.85),1);
    // gvl->insertPointCloudFromFile("myEnvironmentMap", "bowl.binvox", true,
    //                                  gpu_voxels::eBVM_OCCUPIED, true, gpu_voxels::Vector3f(1.0, 0.5, 0.85),1);
    // //gvl->insertPointCloudFromFile("myEnvironmentMap", "box.binvox", true,
    // //                                 gpu_voxels::eBVM_OCCUPIED, true, gpu_voxels::Vector3f(1.0, 0.8, 0.2),0.5);
    x += 0.05;

   
    gvl->visualizeMap("myEnvironmentMap");
}

void GvlOmplPlannerHelper::doVis()
{

    // tell the visualier that the map has changed:

    gvl->visualizeMap("myRobotMap");
    gvl->visualizeMap("myEnvironmentMap");

    gvl->visualizeMap("countingVoxelList");
    gvl->visualizeMap("mySolutionMap");
    gvl->visualizeMap("myQueryMap");
        gvl->clearMap("myRobotMap");
    gvl->visualizeMap("myRobotMap2");
}
void GvlOmplPlannerHelper::getJointStates(double* return_values)
{
        for(int i = 0;i<7;i++)
                return_values[i] = move_joint_values[i];
        
}

void GvlOmplPlannerHelper::visualizeSolution(ob::PathPtr path)
{
    gvl->clearMap("mySolutionMap");

    PERF_MON_SUMMARY_PREFIX_INFO("pose_check");
    PERF_MON_SUMMARY_PREFIX_INFO("motion_check");
    PERF_MON_SUMMARY_PREFIX_INFO("motion_check_lv");

    //std::cout << "Robot consists of " << gvl->getRobot("myUrdfRobot")->getTransformedClouds()->getAccumulatedPointcloudSize() << " points" << std::endl;

    og::PathGeometric* solution = path->as<og::PathGeometric>();
    solution->interpolate();


    for(size_t step = 0; step < solution->getStateCount(); ++step)
    {

        const double *values = solution->getState(step)->as<ob::RealVectorStateSpace::StateType>()->values;

        robot::JointValueMap state_joint_values;
        state_joint_values["panda_joint1"] = values[0];
        state_joint_values["panda_joint2"] = values[1];
        state_joint_values["panda_joint3"] = values[2];
        state_joint_values["panda_joint4"] = values[3];
        state_joint_values["panda_joint5"] = values[4];
        state_joint_values["panda_joint6"] = values[5];
        state_joint_values["panda_joint7"] = values[6];
        // update the robot joints:
        gvl->setRobotConfiguration("myUrdfRobot", state_joint_values);
        // insert the robot into the map:
        gvl->insertRobotIntoMap("myUrdfRobot", "mySolutionMap", BitVoxelMeaning(eBVM_SWEPT_VOLUME_START + (step % 249) ));
    }

    gvl->visualizeMap("mySolutionMap");

}

void GvlOmplPlannerHelper::visualizeRobot(const double *values)
{
        gvl->clearMap("myRobotMap");
        gvl->clearMap("myRobotMap2");
        gvl->clearMap("mySolutionMap");
        gvl->clearMap("myQueryMap");
        robot::JointValueMap state_joint_values;
         state_joint_values["panda_joint1"] = values[0];
        state_joint_values["panda_joint2"] = values[1];
        state_joint_values["panda_joint3"] = values[2];
        state_joint_values["panda_joint4"] = values[3];
        state_joint_values["panda_joint5"] = values[4];
        state_joint_values["panda_joint6"] = values[5];
        state_joint_values["panda_joint7"] = values[6];
        // update the robot joints:
        gvl->setRobotConfiguration("myUrdfRobot", state_joint_values);
        // insert the robot into the map:
       gvl->insertRobotIntoMap("myUrdfRobot", "myRobotMap", eBVM_OCCUPIED);
       gvl->insertRobotIntoMap("myUrdfRobot", "myRobotMap2",BitVoxelMeaning(eBVM_SWEPT_VOLUME_START+180));

        gvl->clearMap("myRobotMap");
    gvl->visualizeMap("myRobotMap2");

}
void GvlOmplPlannerHelper::insertStartAndGoal(const ompl::base::ScopedState<> &start, const ompl::base::ScopedState<> &goal) const
{

    gvl->clearMap("myQueryMap");

    robot::JointValueMap state_joint_values;
    state_joint_values["panda_joint1"] = start[0];
    state_joint_values["panda_joint2"] = start[1];
    state_joint_values["panda_joint3"] = start[2];
    state_joint_values["panda_joint4"] = start[3];
    state_joint_values["panda_joint5"] = start[4];
    state_joint_values["panda_joint6"] = start[5];
    state_joint_values["panda_joint7"] = start[6];

    // update the robot joints:
    gvl->setRobotConfiguration("myUrdfRobot", state_joint_values);
    gvl->insertRobotIntoMap("myUrdfRobot", "myQueryMap", BitVoxelMeaning(eBVM_SWEPT_VOLUME_START));

    state_joint_values["panda_joint1"] = goal[0];
    state_joint_values["panda_joint2"] = goal[1];
    state_joint_values["panda_joint3"] = goal[2];
    state_joint_values["panda_joint4"] = goal[3];
    state_joint_values["panda_joint5"] = goal[4];
    state_joint_values["panda_joint6"] = goal[5];
    state_joint_values["panda_joint7"] = goal[6];
    // update the robot joints:
    gvl->setRobotConfiguration("myUrdfRobot", state_joint_values);
    gvl->insertRobotIntoMap("myUrdfRobot", "myQueryMap", BitVoxelMeaning(eBVM_SWEPT_VOLUME_START+1));

}

bool GvlOmplPlannerHelper::isValid(const ompl::base::State *state) const
{

    PERF_MON_START("inserting");

    std::lock_guard<std::mutex> lock(g_i_mutex);

    gvl->clearMap("myRobotMap");

    const double *values = state->as<ob::RealVectorStateSpace::StateType>()->values;

    robot::JointValueMap state_joint_values;
    state_joint_values["panda_joint1"] = values[0];
    state_joint_values["panda_joint2"] = values[1];
    state_joint_values["panda_joint3"] = values[2];
    state_joint_values["panda_joint4"] = values[3];
    state_joint_values["panda_joint5"] = values[4];
    state_joint_values["panda_joint6"] = values[5];
    state_joint_values["panda_joint7"] = values[6];
    // update the robot joints:
    gvl->setRobotConfiguration("myUrdfRobot", state_joint_values);
    // insert the robot into the map:
    gvl->insertRobotIntoMap("myUrdfRobot", "myRobotMap", eBVM_OCCUPIED);

    PERF_MON_SILENT_MEASURE_AND_RESET_INFO_P("insert", "Pose Insertion", "pose_check");

    PERF_MON_START("coll_test");
    size_t num_colls_pc = gvl->getMap("myRobotMap")->as<voxelmap::ProbVoxelMap>()->collideWith(gvl->getMap("myEnvironmentMap")->as<voxelmap::ProbVoxelMap>(), 0.7f);
    PERF_MON_SILENT_MEASURE_AND_RESET_INFO_P("coll_test", "Pose Collsion", "pose_check");

    //std::cout << "Validity check on state ["  << values[0] << ", " << values[1] << ", " << values[2] << ", " << values[3] << ", " << values[4] << ", " << values[5] << "] resulting in " <<  num_colls_pc << " colls." << std::endl;

    return num_colls_pc == 0;
}

bool GvlOmplPlannerHelper::checkMotion(const ompl::base::State *s1, const ompl::base::State *s2,
                                       std::pair< ompl::base::State*, double > & lastValid) const
{

    //    std::cout << "LongestValidSegmentFraction = " << stateSpace_->getLongestValidSegmentFraction() << std::endl;
    //    std::cout << "getLongestValidSegmentLength = " << stateSpace_->getLongestValidSegmentLength() << std::endl;
    //    std::cout << "getMaximumExtent = " << stateSpace_->getMaximumExtent() << std::endl;


    std::lock_guard<std::mutex> lock(g_j_mutex);
    gvl->clearMap("myRobotMap");

    /* assume motion starts in a valid configuration so s1 is valid */

    bool result = true;
    int nd = stateSpace_->validSegmentCount(s1, s2);

    //std::cout << "Called interpolating motion_check_lv to evaluate " << nd << " segments" << std::endl;

    PERF_MON_ADD_DATA_NONTIME_P("Num poses in motion", float(nd), "motion_check_lv");
    if (nd > 1)
    {
        /* temporary storage for the checked state */
        ob::State *test = si_->allocState();

        for (int j = 1; j < nd; ++j)
        {
            stateSpace_->interpolate(s1, s2, (double)j / (double)nd, test);
            if (!si_->isValid(test))

            {
                lastValid.second = (double)(j - 1) / (double)nd;
                if (lastValid.first != nullptr)
                    stateSpace_->interpolate(s1, s2, lastValid.second, lastValid.first);
                result = false;
                break;

            }
        }

        si_->freeState(test);

    }

    if (result)
        if (!si_->isValid(s2))
        {
            lastValid.second = (double)(nd - 1) / (double)nd;
            if (lastValid.first != nullptr)
                stateSpace_->interpolate(s1, s2, lastValid.second, lastValid.first);
            result = false;
        }


    if (result)
        valid_++;
    else
        invalid_++;


    return result;
}







/*
void
 GvlOmplPlannerHelper::rosPublishPose(double *values){
        geometry_msgs::PoseStamped output_msg;
        output_msg.pose.position.x = values[0];
        output_msg.pose.position.y = values[1];
        output_msg.pose.position.z = values[2];
        Eigen::Quaternionf q;
        q = Eigen::AngleAxisf(values[3], Eigen::Vector3f::UnitX())
            * Eigen::AngleAxisf(values[4], Eigen::Vector3f::UnitY())
            * Eigen::AngleAxisf(values[5], Eigen::Vector3f::UnitZ());
        output_msg.pose.orientation.x = q.x();
        output_msg.pose.orientation.y = q.y();
        output_msg.pose.orientation.z = q.z();
        output_msg.pose.orientation.w = q.w();

        pub.publish(output_msg);  
}*/
void GvlOmplPlannerHelper::rosPublishJointStates(double *values){
    
move_joint_values[0] = values[0];
move_joint_values[1] = values[1];
move_joint_values[2] = values[2];
move_joint_values[3] = values[3];
move_joint_values[4] = values[4];
move_joint_values[5] = values[5];
move_joint_values[6] = values[6];


}

void GvlOmplPlannerHelper::rosIter(){
 int argc;
 char **argv;
 ros::init(argc,argv,"gpu_voxel_temp");
signal(SIGINT, ctrlchandler);
  signal(SIGTERM, killhandler);

  icl_core::config::GetoptParameter points_parameter("points-topic:", "t",
                                                    "Identifer of the pointcloud topic");
  icl_core::config::GetoptParameter roll_parameter  ("roll:", "r",
                                                    "Camera roll in degrees");
  icl_core::config::GetoptParameter pitch_parameter ("pitch:", "p",
                                                    "Camera pitch in degrees");
  icl_core::config::GetoptParameter yaw_parameter   ("yaw:", "y",
                                                    "Camera yaw in degrees");
  icl_core::config::GetoptParameter voxel_side_length_parameter("voxel_side_length:", "s",
                                                                "Side length of a voxel, default 0.01");
  icl_core::config::GetoptParameter filter_threshold_parameter ("filter_threshold:", "f",
                                                                "Density filter threshold per voxel, default 1");
  icl_core::config::GetoptParameter erode_threshold_parameter  ("erode_threshold:", "e",
                                                                "Erode voxels with fewer occupied neighbors (percentage)");
  icl_core::config::addParameter(points_parameter);
  icl_core::config::addParameter(roll_parameter);
  icl_core::config::addParameter(pitch_parameter);
  icl_core::config::addParameter(yaw_parameter);
  icl_core::config::addParameter(voxel_side_length_parameter);
  icl_core::config::addParameter(filter_threshold_parameter);
  icl_core::config::addParameter(erode_threshold_parameter);
	
  voxel_side_length = icl_core::config::paramOptDefault<float>("voxel_side_length", 0.01f);

  // setup "tf" to transform from camera to world / gpu-voxels coordinates

  //const Vector3f camera_offsets(2, 0, 1); // camera located at y=0, x_max/2, z_max/2
  const Vector3f camera_offsets(1.0f,1.0f,0.84f);  // camera located at y=-0.2m, x_max/2, z_max/2

  float roll = icl_core::config::paramOptDefault<float>("roll", 0.0f) * 3.141592f / 180.0f;
  float pitch = icl_core::config::paramOptDefault<float>("pitch", 0.0f) * 3.141592f / 180.0f;
  float yaw = icl_core::config::paramOptDefault<float>("yaw", 0.0f) * 3.141592f / 180.0f;
  tf = Matrix4f::createFromRotationAndTranslation(Matrix3f::createFromRPY(0+ roll, 0+pitch, 0+  yaw), camera_offsets);

  
  //std::string point_cloud_topic = icl_core::config::paramOptDefault<std::string>("points-topic", "/camera/depth/color/points");
  //LOGGING_INFO(Gpu_voxels, "DistanceROSDemo start. Point-cloud topic: " << point_cloud_topic << endl);

  // Generate a GPU-Voxels instance:
  gvl = gpu_voxels::GpuVoxels::getInstance();
  gvl->initialize(300,300, 500, 0.01);
 

  //Vis Helper
  gvl->addPrimitives(primitive_array::ePRIM_SPHERE, "measurementPoints");

  //PBA
  gvl->addMap(MT_DISTANCE_VOXELMAP, "pbaDistanceVoxmap");
  shared_ptr<DistanceVoxelMap> pbaDistanceVoxmap = dynamic_pointer_cast<DistanceVoxelMap>(gvl->getMap("pbaDistanceVoxmap"));

  gvl->addMap(MT_PROBAB_VOXELMAP, "myEnvironmentMap");
  shared_ptr<ProbVoxelMap> erodeTempVoxmap1 = dynamic_pointer_cast<ProbVoxelMap>(gvl->getMap("myEnvironmentMap"));

  gvl->addMap(MT_COUNTING_VOXELLIST, "countingVoxelList");
  shared_ptr<CountingVoxelList> countingVoxelList = dynamic_pointer_cast<CountingVoxelList>(gvl->getMap("countingVoxelList"));


  // Define two measurement points:
  std::vector<Vector3i> measurement_points;
  measurement_points.push_back(Vector3i(40, 100, 50));
  measurement_points.push_back(Vector3i(160, 100, 50));
  gvl->modifyPrimitives("measurementPoints", measurement_points, 5);

  int filter_threshold = icl_core::config::paramOptDefault<int>("filter_threshold", 0);
  //std::cout << "Remove voxels containing less points than: " << filter_threshold << std::endl;

  float erode_threshold = icl_core::config::paramOptDefault<float>("erode_threshold", 0.0f);
  //std::cout << "Erode voxels with neighborhood occupancy ratio less or equal to: " << erode_threshold << std::endl;


  ros::NodeHandle nh;

  ros::Subscriber sub1 = nh.subscribe("joint_states", 1, rosjointStateCallback); 
std::string point_cloud_topic = icl_core::config::paramOptDefault<std::string>("points-topic", "/point_cloud2");
  ros::Subscriber sub = nh.subscribe<pcl::PointCloud<pcl::PointXYZ> >(point_cloud_topic, 1,roscallback);
   ros::Publisher pub =  nh.advertise<sensor_msgs::JointState>("joint_states", 1000);
   ros::Publisher pub2 =  nh.advertise<sensor_msgs::JointState>("joint_states_desired", 1000);

  ros::Rate r(30);
  size_t iteration = 0;
  new_data_received = true; // call visualize on the first iteration

  //LOGGING_INFO(Gpu_voxels, "start visualizing maps" << endl);
  for(int j = 0;j<100;j++){
  	ros::spinOnce();
	countingVoxelList->insertPointCloud(my_point_cloud, eBVM_OCCUPIED);
    erodeTempVoxmap1->merge(countingVoxelList);
  	erodeTempVoxmap1->insertPointCloud(my_point_cloud, eBVM_OCCUPIED);
  gvl->visualizeMap("myEnvironmentMap");
      r.sleep();

  }

sensor_msgs::JointState jointState;
jointState.name.push_back("panda_joint1");
jointState.name.push_back("panda_joint2");
jointState.name.push_back("panda_joint3");
jointState.name.push_back("panda_joint4");
jointState.name.push_back("panda_joint5");
jointState.name.push_back("panda_joint6");
jointState.name.push_back("panda_joint7");
jointState.position.push_back(0.0);
jointState.position.push_back(0.0);
jointState.position.push_back(0.0);
jointState.position.push_back(0.0);
jointState.position.push_back(0.0);
jointState.position.push_back(0.0);
jointState.position.push_back(0.0);

  while (ros::ok())
  {
    ros::spinOnce();

    //LOGGING_DEBUG(Gpu_voxels, "START iteration" << endl);
        
        jointState.header.stamp = ros::Time::now();
        jointState.position[0] = move_joint_values[0];
        jointState.position[1] = move_joint_values[1];
        jointState.position[2] = move_joint_values[2];
        jointState.position[3] = move_joint_values[3];
        jointState.position[4] = move_joint_values[4];
        jointState.position[5] = move_joint_values[5];
        jointState.position[6] = move_joint_values[6];
       // std::cout<<"&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&"<<std::endl;
        //std::cout<<jointState.position[0]<<","<<jointState.position[1]<<","<<jointState.position[2]<<","<<jointState.position[3]<<","<<jointState.position[4]<<","<<jointState.position[5]<<","<<jointState.position[6]<<std::endl;
        
        pub.publish(jointState);
        pub2.publish(jointState);

       // std::cout<<"&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&"<<std::endl;

    // visualize new pointcloud if there is new data
    if (new_data_received) 
    {
      new_data_received = false;
      iteration++;

      countingVoxelList->clearMap();
      // Insert the CAMERA data (now in world coordinates) into the list
      countingVoxelList->insertPointCloud(my_point_cloud, eBVM_OCCUPIED);
       gvl->visualizeMap("countingVoxelList");
       gvl->visualizeMap("myEnvironmentMap");

      
    }
 gvl->insertPointCloudFromFile("myEnvironmentMap", "table.binvox", true,
                                      gpu_voxels::eBVM_OCCUPIED, true, gpu_voxels::Vector3f(0.85, 0.5, 0.0),1);
    r.sleep();
  }

  //LOGGING_INFO(Gpu_voxels, "shutting down" << endl);

  exit(EXIT_SUCCESS);
}

bool GvlOmplPlannerHelper::checkMotion(const ompl::base::State *s1, const ompl::base::State *s2) const
{
    std::lock_guard<std::mutex> lock(g_i_mutex);
    gvl->clearMap("myRobotMap");


    //        std::cout << "LongestValidSegmentFraction = " << stateSpace_->getLongestValidSegmentFraction() << std::endl;
    //        std::cout << "getLongestValidSegmentLength = " << stateSpace_->getLongestValidSegmentLength() << std::endl;
    //        std::cout << "getMaximumExtent = " << stateSpace_->getMaximumExtent() << std::endl;



    /* assume motion starts in a valid configuration so s1 is valid */

    bool result = true;
    int nd = stateSpace_->validSegmentCount(s1, s2);

    // not required with ProbabVoxels:
    //    if(nd > 249)
    //    {
    //        std::cout << "Too many intermediate states for BitVoxels" << std::endl;
    //        exit(1);
    //    }

    if (nd > 1)
    {
        PERF_MON_START("inserting");

        /* temporary storage for the checked state */
        ob::State *test = si_->allocState();

        for (int j = 1; j < nd; ++j)
        {
            stateSpace_->interpolate(s1, s2, (double)j / (double)nd, test);


            const double *values = test->as<ob::RealVectorStateSpace::StateType>()->values;

            robot::JointValueMap state_joint_values;
	        state_joint_values["panda_joint1"] = values[0];
	        state_joint_values["panda_joint2"] = values[1];
	        state_joint_values["panda_joint3"] = values[2];
	        state_joint_values["panda_joint4"] = values[3];
	        state_joint_values["panda_joint5"] = values[4];
	        state_joint_values["panda_joint6"] = values[5];
	        state_joint_values["panda_joint7"] = values[6];

            // update the robot joints:
            gvl->setRobotConfiguration("myUrdfRobot", state_joint_values);
            // insert the robot into the map:
            gvl->insertRobotIntoMap("myUrdfRobot", "myRobotMap", eBVM_OCCUPIED);

        }
        PERF_MON_ADD_DATA_NONTIME_P("Num poses in motion", float(nd), "motion_check");

        si_->freeState(test);

        PERF_MON_SILENT_MEASURE_AND_RESET_INFO_P("insert", "Motion Insertion", "motion_check");

        //gvl->visualizeMap("myRobotMap");
        PERF_MON_START("coll_test");
        size_t num_colls_pc = gvl->getMap("myRobotMap")->as<voxelmap::ProbVoxelMap>()->collideWith(gvl->getMap("myEnvironmentMap")->as<voxelmap::ProbVoxelMap>(), 0.7f);

        std::cout << "CheckMotion1 for " << nd << " segments. Resulting in " << num_colls_pc << " colls." << std::endl;
        PERF_MON_SILENT_MEASURE_AND_RESET_INFO_P("coll_test", "Pose Collsion", "motion_check");

        result = (num_colls_pc == 0);

    }


    if (result)
        valid_++;
    else
        invalid_++;


    return result;


}
