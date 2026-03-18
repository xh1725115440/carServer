#pragma once

#include <mysql.h>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <time.h>
#include <boost/asio.hpp>
#include <../Base/json.hpp>
#include "MegParse.h"
#include "../Base/Log.h"
#define MYSQL_IP  "127.0.0.1"    	   	   // 数据库IP
#define MYSQL_PWD "Tr708708708"            // 数据库密码
#define MYSQL_USER "Car" 			   // 数据库账号
#define MYSQL_NAME "car"      		   // 数据库名称

class TrDatabase
{
public:
	static TrDatabase* createNew();
	TrDatabase();
	bool init_db();
	void handle_04(struct isr_mess*& isr_mess_reg);
	void handle_05(struct sap_mess*& sap_mess_reg);
	void handle_06(struct SAP_DATA*& sap_data);
	void handle_33(struct sap_data_33*& sap_data);
	void handle(struct Car_DATA*& car_data);
	std::string handle_request(const std::string& request);
	

private:
	void insert_total(struct Car_DATA*& p);
	void update_total(struct isr_mess*& p);
	void insert_data(struct Car_DATA*& p);
	void update_isr(struct sap_mess*& p);
	void insert_sap(struct SAP_DATA*& q);

private:
	int state;
	char st_query[4096] = {};
	int flag = 0;
	std::string temp_sql;
	MYSQL db_g2020;
	MYSQL* connection;
	MYSQL_RES* res;
	MYSQL_ROW row;

};
