
#include "stdafx.h"
#include <iostream>
#include <fstream>
#include<string>

#include "config.h"
#include "Calibrate.h"
#include "PairAlign.h"

int config_num[50];

void ArgvConfig()
{
	Get_ConfigNum();
	board_size.width = config_num[0];
	board_size.height = config_num[1];
	square_size.width = config_num[2];
	square_size.height = config_num[3];
	total_clude = config_num[4];
	Registration_flag = config_num[5];
}

void Get_ConfigNum()
{
	std::string a[50];              //���� string ���ͣ���100�е��ı�����Ҫ������ 

	int i = 0;
	std::ifstream infile;

	infile.open("./config/config.txt", std::ios::in);

	while (!infile.eof())            // ��δ���ļ�����һֱѭ�� 
	{
		getline(infile, a[i], '\n');//��ȡһ�У��Ի��з����������� a[] ��
		i++;                    //��һ��
	}
	for (int ii = 0; ii<i; ii++)        // ��ʾ��ȡ��txt���� 
	{
		std::cout << a[ii] << std::endl;
		std::string::size_type position = a[ii].find(":");
		a[ii]=a[ii].substr(position+1);
		config_num[ii] = std::atoi(a[ii].c_str());
	}
	infile.close();
}