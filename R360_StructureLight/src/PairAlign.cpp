
#include "stdafx.h"
#include "PairAlign.h"

#include <boost/make_shared.hpp> //����ָ��
//��/����
#include <pcl/point_types.h>
#include <pcl/point_cloud.h>
#include <pcl/point_representation.h>
//pcd�ļ�����/���
#include <pcl/io/pcd_io.h>
//�˲�
#include <pcl/filters/voxel_grid.h>
#include <pcl/filters/filter.h>
//����
#include <pcl/features/normal_3d.h>
//��׼
#include <pcl/registration/icp.h> //ICP����
#include <pcl/registration/icp_nl.h>
#include <pcl/registration/transforms.h>
//���ӻ�
#include <pcl/visualization/pcl_visualizer.h>

//�����ռ�
using pcl::visualization::PointCloudColorHandlerGenericField;
using pcl::visualization::PointCloudColorHandlerCustom;


//ȫ�ֱ���
//���ӻ�����
pcl::visualization::PCLVisualizer *p;
//�������������������ӻ����ڷֳ�����������
int vp_1, vp_2;




// �����µĵ��﷽ʽ< x, y, z, curvature > ����+����
class MyPointRepresentation : public pcl::PointRepresentation <PointNormalT> //�̳й�ϵ
{
	using pcl::PointRepresentation<PointNormalT>::nr_dimensions_;
public:
	MyPointRepresentation()
	{
		//ָ��ά��
		nr_dimensions_ = 4;
	}

	//���غ���copyToFloatArray���Զ����Լ�����������
	virtual void copyToFloatArray(const PointNormalT &p, float * out) const
	{
		//< x, y, z, curvature > ����xyz������
		out[0] = p.x;
		out[1] = p.y;
		out[2] = p.z;
		out[3] = p.curvature;
	}
};



//�ڴ��ڵ����������򵥵���ʾԴ���ƺ�Ŀ�����
void showCloudsLeft(const PointCloud::Ptr cloud_target, const PointCloud::Ptr cloud_source)
{
	p->removePointCloud("vp1_target"); //���ݸ�����ID������Ļ��ȥ��һ�����ơ�������ID
	p->removePointCloud("vp1_source"); //
	PointCloudColorHandlerCustom<PointT> tgt_h(cloud_target, 0, 255, 0); //Ŀ�������ɫ
	PointCloudColorHandlerCustom<PointT> src_h(cloud_source, 255, 0, 0); //Դ���ƺ�ɫ
	p->addPointCloud(cloud_target, tgt_h, "vp1_target", vp_1); //���ص���
	p->addPointCloud(cloud_source, src_h, "vp1_source", vp_1);
	PCL_INFO("Press q to begin the registration.\n"); //������������ʾ��ʾ��Ϣ
	p->spin();
}


//�ڴ��ڵ����������򵥵���ʾԴ���ƺ�Ŀ�����
void showCloudsRight(const PointCloudWithNormals::Ptr cloud_target, const PointCloudWithNormals::Ptr cloud_source)
{
	p->removePointCloud("source"); //���ݸ�����ID������Ļ��ȥ��һ�����ơ�������ID
	p->removePointCloud("target");
	PointCloudColorHandlerGenericField<PointNormalT> tgt_color_handler(cloud_target, "curvature"); //Ŀ����Ʋ�ɫ���
	if (!tgt_color_handler.isCapable())
		PCL_WARN("Cannot create curvature color handler!");
	PointCloudColorHandlerGenericField<PointNormalT> src_color_handler(cloud_source, "curvature"); //Դ���Ʋ�ɫ���
	if (!src_color_handler.isCapable())
		PCL_WARN("Cannot create curvature color handler!");
	p->addPointCloud(cloud_target, tgt_color_handler, "target", vp_2); //���ص���
	p->addPointCloud(cloud_source, src_color_handler, "source", vp_2);
	p->spinOnce();
}


// ��ȡһϵ�е�PCD�ļ���ϣ����׼�ĵ����ļ���
// ����argc ����������������main()��
// ����argv �������б�����main()��
// ����models �������ݼ��Ľ������
void loadData(int argc, char **argv, std::vector<PCD, Eigen::aligned_allocator<PCD> > &models)
{
	std::string extension(".pcd"); //��������ʼ��string���ͱ���extension����ʾ�ļ���׺��
	// ͨ�������ļ�������ȡpcd�ļ�
	for (int i = 1; i < argc; i++) //�������е��ļ������Թ���������
	{
		std::string fname = std::string(argv[i]);
		if (fname.size() <= extension.size()) //�ļ����ĳ����Ƿ����Ҫ��
			continue;

		std::transform(fname.begin(), fname.end(), fname.begin(), (int(*)(int))tolower); //��ĳ����(Сд��ĸ��)Ӧ����ָ����Χ��ÿ��Ԫ��
		//����ļ��Ƿ���pcd�ļ�
		if (fname.compare(fname.size() - extension.size(), extension.size(), extension) == 0)
		{
			// ��ȡ���ƣ������浽models
			PCD m;
			m.f_name = argv[i];
			pcl::io::loadPCDFile(argv[i], *m.cloud); //��ȡ��������
			//ȥ�������е�NaN�㣨xyz����NaN��
			std::vector<int> indices; //����ȥ���ĵ������
			pcl::removeNaNFromPointCloud(*m.cloud, *m.cloud, indices); //ȥ�������е�NaN��

			models.push_back(m);
		}
	}
}


//�򵥵���׼һ�Ե������ݣ������ؽ��
//����cloud_src  Դ����
//����cloud_tgt  Ŀ�����
//����output     �������
//����final_transform �ɶԱ任����
//����downsample �Ƿ��²���
void pairAlign(const PointCloud::Ptr cloud_src, const PointCloud::Ptr cloud_tgt, PointCloud::Ptr output, Eigen::Matrix4f &final_transform, bool downsample)
{
	//
	//Ϊ��һ���Ժ��ٶȣ��²���
	// \note enable this for large datasets
	PointCloud::Ptr src(new PointCloud); //��������ָ��
	PointCloud::Ptr tgt(new PointCloud);
	pcl::VoxelGrid<PointT> grid; //VoxelGrid ��һ�������ĵ��ƣ��ۼ���һ���ֲ���3D������,���²������˲���������
	if (downsample) //�²���
	{
		grid.setLeafSize(0.05, 0.05, 0.05); //������Ԫ�����Ҷ�Ӵ�С
		//�²��� Դ����
		grid.setInputCloud(cloud_src); //�����������
		grid.filter(*src); //�²������˲������洢��src��
		//�²��� Ŀ�����
		grid.setInputCloud(cloud_tgt);
		grid.filter(*tgt);
	}
	else //���²���
	{
		src = cloud_src; //ֱ�Ӹ���
		tgt = cloud_tgt;
	}

	//��������ķ�����������
	PointCloudWithNormals::Ptr points_with_normals_src(new PointCloudWithNormals); //����Դ����ָ�루ע�������Ͱ�������ͷ�������
	PointCloudWithNormals::Ptr points_with_normals_tgt(new PointCloudWithNormals); //����Ŀ�����ָ�루ע�������Ͱ�������ͷ�������
	pcl::NormalEstimation<PointT, PointNormalT> norm_est; //�ö������ڼ��㷨����
	pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>()); //����kd�������ڼ��㷨��������������
	norm_est.setSearchMethod(tree); //������������
	norm_est.setKSearch(30); //��������ڵ�����
	norm_est.setInputCloud(src); //����������
	norm_est.compute(*points_with_normals_src); //���㷨���������洢��points_with_normals_src
	pcl::copyPointCloud(*src, *points_with_normals_src); //���Ƶ��ƣ����꣩��points_with_normals_src����������ͷ�������
	norm_est.setInputCloud(tgt); //��3�м���Ŀ����Ƶķ�������ͬ��
	norm_est.compute(*points_with_normals_tgt);
	pcl::copyPointCloud(*tgt, *points_with_normals_tgt);

	//����һ�� �Զ�����﷽ʽ�� ʵ��
	MyPointRepresentation point_representation;
	//��Ȩ����ά�ȣ��Ժ�����xyz����ƽ��
	float alpha[4] = { 1.0, 1.0, 1.0, 1.0 };
	point_representation.setRescaleValues(alpha); //��������ֵ����������ʱʹ�ã�

	//����������ICP���� �����ò���
	pcl::IterativeClosestPointNonLinear<PointNormalT, PointNormalT> reg; //����������ICP����ICP���壬ʹ��Levenberg-Marquardt���Ż���
	reg.setTransformationEpsilon(1e-6); //���������������������Ż���
	//***** ע�⣺�����Լ����ݿ�Ĵ�С���ڸò���
	reg.setMaxCorrespondenceDistance(0.1);  //���ö�Ӧ��֮��������루0.1m��,����׼�����У����Դ��ڸ���ֵ�ĵ�
	reg.setPointRepresentation(boost::make_shared<const MyPointRepresentation>(point_representation)); //���õ���
	//����Դ���ƺ�Ŀ�����
	//reg.setInputSource (points_with_normals_src); //�汾�����ϣ�ʹ����������
	reg.setInputCloud(points_with_normals_src); //����������ƣ����任�ĵ��ƣ�
	reg.setInputTarget(points_with_normals_tgt); //����Ŀ�����
	reg.setMaximumIterations(2); //�����ڲ��Ż��ĵ�������

	// Run the same optimization in a loop and visualize the results
	Eigen::Matrix4f Ti = Eigen::Matrix4f::Identity(), prev, targetToSource;
	PointCloudWithNormals::Ptr reg_result = points_with_normals_src; //���ڴ洢���������+��������

	for (int i = 0; i < 30; ++i) //����
	{
		PCL_INFO("Iteration Nr. %d.\n", i); //��������ʾ�����Ĵ���
		//������ƣ����ڿ��ӻ�
		points_with_normals_src = reg_result; //
		//����
		//reg.setInputSource (points_with_normals_src);
		reg.setInputCloud(points_with_normals_src); //��������������ƣ����任�ĵ��ƣ�����Ϊ������һ�ε������Ѿ������任��
		reg.align(*reg_result); //���루��׼����������

		Ti = reg.getFinalTransformation() * Ti; //�ۻ���ÿ�ε����ģ��任����
		//�����α任���ϴα任��������ֵС��ͨ����С���Ķ�Ӧ�����ķ�������һ��ϸ��
		if (fabs((reg.getLastIncrementalTransformation() - prev).sum()) < reg.getTransformationEpsilon())
			reg.setMaxCorrespondenceDistance(reg.getMaxCorrespondenceDistance() - 0.001); //��С��Ӧ��֮��������루�������ù���
		prev = reg.getLastIncrementalTransformation(); //��һ�α任�����
		//��ʾ��ǰ��׼״̬���ڴ��ڵ����������򵥵���ʾԴ���ƺ�Ŀ�����
		showCloudsRight(points_with_normals_tgt, points_with_normals_src);
	}

	targetToSource = Ti.inverse(); //�����Ŀ����Ƶ�Դ���Ƶı任����
	pcl::transformPointCloud(*cloud_tgt, *output, targetToSource); //��Ŀ����� �任�ص� Դ����֡����һ���Ǿ���

	p->removePointCloud("source"); //���ݸ�����ID������Ļ��ȥ��һ�����ơ�������ID
	p->removePointCloud("target");
	PointCloudColorHandlerCustom<PointT> cloud_tgt_h(output, 0, 255, 0); //���õ�����ʾ��ɫ����ͬ
	PointCloudColorHandlerCustom<PointT> cloud_src_h(cloud_src, 255, 0, 0);
	p->addPointCloud(output, cloud_tgt_h, "target", vp_2); //��ӵ������ݣ���ͬ
	p->addPointCloud(cloud_src, cloud_src_h, "source", vp_2);

	PCL_INFO("Press q to continue the registration.\n");
	p->spin();

	p->removePointCloud("source");
	p->removePointCloud("target");

	//add the source to the transformed target
	*output += *cloud_src; // ƴ�ӵ���ͼ���ĵ㣩������Ŀ���������Ƶĵ�����

	final_transform = targetToSource; //���յı任��Ŀ����Ƶ�Դ���Ƶı任����
}
