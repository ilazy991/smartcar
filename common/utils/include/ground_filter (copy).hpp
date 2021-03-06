// #pragma once

// #ifndef __DDL_GROUND_FILTER__
// #define __DDL_GROUND_FILTER__
#ifndef FILTER_GROUND_H
#define FILTER_GROUND_H

#include <ros/ros.h>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_types.h>
#include <pcl/conversions.h>
#include <pcl_ros/transforms.h>

#include <pcl/filters/extract_indices.h>

#include <sensor_msgs/PointCloud2.h>
#include <geometry_msgs/PoseStamped.h>
#include <tf/transform_listener.h>
#include <omp.h>

namespace utils{

class RayGroundFilter
{

using PointNoI = pcl::PointXYZ;

private:
  int SENSOR_MODEL;
  double SENSOR_HEIGHT;
  // double default_cluster_height;
  // double default_plane_radius;

  double RADIAL_DIVIDER_ANGLE;  // default: 0.18

  double local_max_slope_;     // degree default: 8 //max slope of the ground between points, degree
  double general_max_slope_;  // degree  default: 5 //max slope of the ground in entire point cloud, degree

  double CLIP_HEIGHT;  //截取掉高于雷达自身xx米的点
  double minX;  // 提取该范围内的点
  double maxX;
  double minY;
  double maxY;

  double MIN_DISTANCE; // default: 2.4

  double concentric_divider_distance_;  // default: 0.01 //0.1 meters default
  double min_height_threshold_;  // default: 0.05

  double reclass_distance_threshold_;  // default: 0.2
  
  // DEBUG
  bool debug_pub_all;
  std::string debug_map_topic;

  bool is_clip_height;
  
  struct PointXYZIRTColor
  {
    PointNoI point;

    float radius; //cylindric coords on XY Plane
    float theta;  //angle deg on XY plane

    size_t radial_div;     //index of the radial divsion to which this point belongs to
    size_t concentric_div; //index of the concentric division to which this points belongs to

    size_t original_index; //index of this point in the source pointcloud
  };
  typedef std::vector<PointXYZIRTColor> PointCloudXYZIRTColor;

  size_t radial_dividers_num_;
  size_t concentric_dividers_num_;

public:
  RayGroundFilter(){
    int SENSOR_MODEL = 16;
    double SENSOR_HEIGHT = 0.3;
    double local_max_slope_ = 8.0;
    double general_max_slope_ = 5.0;
    double min_height_threshold_ = 0.05;
    double reclass_distance_threshold_ = 0.2;
    double RADIAL_DIVIDER_ANGLE = 0.18;
    double concentric_divider_distance_ = 0.01;
    double MIN_DISTANCE = 2.4;

    double CLIP_HEIGHT = 1.0;
    double minX = -5.0;  
    double maxX = 20.0;
    double minY = -10.0;
    double maxY = 10.0;
  };
  ~RayGroundFilter(){};

    void clip_above(double clip_height, const pcl::PointCloud<PointNoI>::Ptr in,
                                const pcl::PointCloud<PointNoI>::Ptr out,
                                const pcl::PointIndices::Ptr up_indices)
    {
        pcl::ExtractIndices<PointNoI> cliper;

        cliper.setInputCloud(in);
        pcl::PointIndices indices;
#pragma omp for  // #pragma omp for语法OpenMP的并行化语法，即希望通过OpenMP并行化执行这条语句后的for循环，从而起到加速的效果。
        for (size_t i = 0; i < in->points.size(); i++)
        {
            if (in->points[i].z > clip_height)
            {
                indices.indices.push_back(i);
                up_indices->indices.push_back(i);
            }
        }
        cliper.setIndices(boost::make_shared<pcl::PointIndices>(indices));
        cliper.setNegative(true); //ture to remove the indices
        cliper.filter(*out);
    }

    void remove_close_pt(double min_distance, const pcl::PointCloud<PointNoI>::Ptr in,
                                    const pcl::PointCloud<PointNoI>::Ptr out,
                                    const pcl::PointIndices::Ptr target_indices)
    {
        pcl::ExtractIndices<PointNoI> cliper;

        cliper.setInputCloud(in);
        pcl::PointIndices indices;
    #pragma omp for
        for (size_t i = 0; i < in->points.size(); i++)
        {
            // double distance = sqrt(in->points[i].x * in->points[i].x + in->points[i].y * in->points[i].y);

            // if (distance < min_distance)
            // {
            //     indices.indices.push_back(i);
            // }
            double x = in->points[i].x;
            double y = in->points[i].y;
            double z = in->points[i].z;
            if(minX > x || x > maxX || minY > y || y > maxY){
                indices.indices.push_back(i);
            }

            // else if(z < default_cluster_height - SENSOR_HEIGHT){   // 过滤小半径范围内的地面点： 因为这些点在射线上的距离很近;且近距离的地面点可简单通过阈值过滤掉
            //     double distance = sqrt(x * x + y * y);
            //     if(distance < default_plane_radius){
            //         indices.indices.push_back(i);
            //     }
            // }
        }
        cliper.setIndices(boost::make_shared<pcl::PointIndices>(indices));
        cliper.setNegative(true); //ture to remove the indices
        cliper.filter(*out);
    }

    /*!
    *
    * @param[in] in_cloud Input Point Cloud to be organized in radial segments
    * @param[out] out_organized_points Custom Point Cloud filled with XYZRTZColor data
    * @param[out] out_radial_divided_indices Indices of the points in the original cloud for each radial segment
    * @param[out] out_radial_ordered_clouds Vector of Points Clouds, each element will contain the points ordered
    */
    void XYZI_to_RTZColor(const pcl::PointCloud<PointNoI>::Ptr in_cloud,
                                    PointCloudXYZIRTColor &out_organized_points,
                                    std::vector<pcl::PointIndices> &out_radial_divided_indices,
                                    std::vector<PointCloudXYZIRTColor> &out_radial_ordered_clouds)
    {
        std::cout << "ground_filter: incloud size: " << in_cloud->points.size() << std::endl;
        out_organized_points.resize(in_cloud->points.size());
        out_radial_divided_indices.clear();
        
        std::cout << "radial divider num " << radial_dividers_num_ << std::endl;
        out_radial_divided_indices.resize(radial_dividers_num_);
        out_radial_ordered_clouds.resize(radial_dividers_num_);
        std::cout << "ground_filter: XYZI_to_RTZColor-1" << std::endl;

        for (size_t i = 0; i < in_cloud->points.size(); i++)
        {
            PointXYZIRTColor new_point;
            auto radius = (float)sqrt(
                in_cloud->points[i].x * in_cloud->points[i].x + in_cloud->points[i].y * in_cloud->points[i].y);
            auto theta = (float)atan2(in_cloud->points[i].y, in_cloud->points[i].x) * 180 / M_PI;
            if (theta < 0)
            {
                theta += 360;
            }
            //角度的微分
            auto radial_div = (size_t)floor(theta / RADIAL_DIVIDER_ANGLE);
            //半径的微分
            auto concentric_div = (size_t)floor(fabs(radius / concentric_divider_distance_));

            new_point.point = in_cloud->points[i];
            new_point.radius = radius;
            new_point.theta = theta;
            new_point.radial_div = radial_div;
            new_point.concentric_div = concentric_div;
            new_point.original_index = i;

            out_organized_points[i] = new_point;

            //radial divisions更加角度的微分组织射线
            out_radial_divided_indices[radial_div].indices.push_back(i);

            out_radial_ordered_clouds[radial_div].push_back(new_point);

        } //end for

        //将同一根射线上的点按照半径（距离）排序
    #pragma omp for
        for (size_t i = 0; i < radial_dividers_num_; i++)
        {
            std::sort(out_radial_ordered_clouds[i].begin(), out_radial_ordered_clouds[i].end(),
                    [](const PointXYZIRTColor &a, const PointXYZIRTColor &b) { return a.radius < b.radius; });
        }
    }

    /*!
    * Classifies Points in the PointCoud as Ground and Not Ground
    * @param in_radial_ordered_clouds Vector of an Ordered PointsCloud ordered by radial distance from the origin
    * @param out_ground_indices Returns the indices of the points classified as ground in the original PointCloud
    * @param out_no_ground_indices Returns the indices of the points classified as not ground in the original PointCloud
    */
    void classify_pc(std::vector<PointCloudXYZIRTColor> &in_radial_ordered_clouds,
                                pcl::PointIndices &out_ground_indices,
                                pcl::PointIndices &out_no_ground_indices)
    {
        out_ground_indices.indices.clear();
        out_no_ground_indices.indices.clear();
    #pragma omp for
        for (size_t i = 0; i < in_radial_ordered_clouds.size(); i++) //sweep through each radial division 遍历每一根射线
        {
            float prev_radius = 0.f;
            float prev_height = -SENSOR_HEIGHT;
            bool prev_ground = false;
            bool current_ground = false;
            for (size_t j = 0; j < in_radial_ordered_clouds[i].size(); j++) //loop through each point in the radial div
            {
                float points_distance = in_radial_ordered_clouds[i][j].radius - prev_radius;
                float height_threshold = tan(DEG2RAD(local_max_slope_)) * points_distance;
                float current_height = in_radial_ordered_clouds[i][j].point.z;
                float general_height_threshold = tan(DEG2RAD(general_max_slope_)) * in_radial_ordered_clouds[i][j].radius;

                //for points which are very close causing the height threshold to be tiny, set a minimum value
                if (points_distance < concentric_divider_distance_ || height_threshold < min_height_threshold_)
                {
                    height_threshold = min_height_threshold_;
                }

                //check current point height against the LOCAL threshold (previous point)
                if (current_height <= (prev_height + height_threshold) && current_height >= (prev_height - height_threshold))
                {
                    //Check again using general geometry (radius from origin) if previous points wasn't ground
                    if (!prev_ground)
                    {
                        if (current_height <= (-SENSOR_HEIGHT + general_height_threshold) && current_height >= (-SENSOR_HEIGHT - general_height_threshold))
                        {
                            current_ground = true;
                        }
                        else
                        {
                            current_ground = false;
                        }
                    }
                    else
                    {
                        current_ground = true;
                    }
                }
                else
                {
                    //check if previous point is too far from previous one, if so classify again
                    if (points_distance > reclass_distance_threshold_ &&
                        (current_height <= (-SENSOR_HEIGHT + height_threshold) && current_height >= (-SENSOR_HEIGHT - height_threshold)))
                    {
                        current_ground = true;
                    }
                    else
                    {
                        current_ground = false;
                    }
                }

                if (current_ground)
                {
                    out_ground_indices.indices.push_back(in_radial_ordered_clouds[i][j].original_index);
                    prev_ground = true;
                }
                else
                {
                    out_no_ground_indices.indices.push_back(in_radial_ordered_clouds[i][j].original_index);
                    prev_ground = false;
                }

                prev_radius = in_radial_ordered_clouds[i][j].radius;
                prev_height = in_radial_ordered_clouds[i][j].point.z;
            }
        }
    }


    void convert(const pcl::PointCloud<PointNoI>::Ptr &current_pc_ptr,
                                const pcl::PointCloud<PointNoI>::Ptr &no_ground_cloud_ptr)
    {   
        pcl::PointIndices::Ptr up_indices(new pcl::PointIndices());
        up_indices->indices.clear();
        pcl::PointCloud<PointNoI>::Ptr cliped_pc_ptr(new pcl::PointCloud<PointNoI>());
        clip_above(CLIP_HEIGHT, current_pc_ptr, cliped_pc_ptr, up_indices);

        pcl::PointCloud<PointNoI>::Ptr up_pc_ptr(new pcl::PointCloud<PointNoI>);
        pcl::ExtractIndices<PointNoI> extract_adove;
        extract_adove.setInputCloud(current_pc_ptr);
        extract_adove.setIndices(boost::make_shared<pcl::PointIndices>(*up_indices));
        extract_adove.setNegative(false); //true removes the indices, false save the indices
        extract_adove.filter(*up_pc_ptr);

        pcl::PointCloud<PointNoI>::Ptr remove_close(new pcl::PointCloud<PointNoI>);
        remove_close_pt(MIN_DISTANCE, cliped_pc_ptr, remove_close, nullptr);
        
        PointCloudXYZIRTColor organized_points;
        std::vector<pcl::PointIndices> radial_division_indices;
        std::vector<pcl::PointIndices> closest_indices;
        std::vector<PointCloudXYZIRTColor> radial_ordered_clouds;

        radial_dividers_num_ = ceil(360 / RADIAL_DIVIDER_ANGLE);
        std::cout << "ground_filter: RADIAL_DIVIDER_ANGLE" << RADIAL_DIVIDER_ANGLE << std::endl;
        std::cout << "ground_filter: remove_close" << remove_close->points.size() << std::endl;

        XYZI_to_RTZColor(remove_close, organized_points,
                        radial_division_indices, radial_ordered_clouds);
        std::cout << "ground_filter: set5" << std::endl;

        pcl::PointIndices ground_indices, no_ground_indices;

        classify_pc(radial_ordered_clouds, ground_indices, no_ground_indices);
        std::cout << "ground_filter: set6" << std::endl;

        pcl::ExtractIndices<PointNoI> extract_ground;
        extract_ground.setInputCloud(remove_close);
        extract_ground.setIndices(boost::make_shared<pcl::PointIndices>(ground_indices));
        std::cout << "ground_filter: set8" << std::endl;

        extract_ground.setNegative(true); //true removes the indices, false leaves only the indices
        extract_ground.filter(*no_ground_cloud_ptr);
        std::cout << "ground_filter: set9" << std::endl;
        if(!is_clip_height){
            *no_ground_cloud_ptr += *up_pc_ptr;
        }
    }

    void setIfClipHeight(bool yn){
        is_clip_height = yn;
    }
};//end 

};

#endif
