#!/usr/bin/env python
#
#File: call_select.py
#Author: R.krempaska
#Description: The program should be called from bootinfo.
#	It calls a ioc_select.php page which does select query as ssrm_public. 
#
import httplib, sys, os, urllib
from urllib import quote_plus

sqlquery=sys.argv[1]
#print sqlquery
try:
	conn=httplib.HTTP("pc3839.psi.ch")
	req='/testplan/IOC_INFOS/ioc_select.php?SQLQUER='+quote_plus(sqlquery)
	conn.putrequest('GET', req)
        conn.endheaders()
        errcode, errmsg, headers = conn.getreply()
        f = conn.getfile()
        data = f.read()
        f.close()
        print data
except:
        out=open("/tmp/ioc_select.log", 'aw')
        #need to write more - time of unsuccess, , author, etc
        out.write(data)
	out.close()
