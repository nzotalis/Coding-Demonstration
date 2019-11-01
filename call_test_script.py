#!/usr/bin/python
import os
from datetime import datetime as d

def main():
    #generate the xml script that sipp will use to test the call, name it after the current time
    now = currentTime()
    scriptname = '%s_script.xml' % now
    tracename = '%s_trace.txt' % now

    os.system('touch %s' % scriptname)
    script = open(scriptname, 'w')
    text = generatescript("server_name_here", "username_here", "password_here", "test_phone_number_here")
    script.write(text)
    script.close()
    
    #launch the script from command line and dump screen into trace.txt when finished
    os.system('sipp -bg -aa -sf %s testcentsix.coredial.com -m 1 -trace_screen -screen_file %s' % (scriptname, tracename))

    #wait for log files to be generated
    while True:
        try:
            #grab the information from log file to be parsed
            logs = open(tracename, 'r')
            lines = logs.readlines()
            logs.close()

            if lines != []:
                break
        except:
            pass

    #delete log file and script from directory
    os.system('rm %s' % tracename)
    os.system('rm %s' % scriptname)

    #parse log file to find the part where it shows how many successful calls we had, and how many total calls we generated
    for line in lines:
        if 'Total Call created' in line:
            totalcallline = line.split('|')

        if 'Successful call' in line:
            successline = line.split('|')

    print(totalcallline[2].strip())
    print(successline[2].strip())

#this just returns the current time as a string for naming the script and trace files
def currentTime():
    n = str(d.now())
    n = n.replace(' ', '_')
    n = n.replace('-', '_')
    n = n.replace(':', '_')
    n = n.replace('.', '_')
    return n
#I wouldn't even look down here. This is just a garbage method that returns the xml script as one huge string and inserts the phone number, server, username, pw, etc.
def generatescript(service, username, password, number):
    
    s = '''<?xml version="1.0" encoding="iso-8859-1"?>
<!DOCTYPE scenario SYSTEM "sipp.dtd">

<scenario name="My script">
  <send>
    <![CDATA[

    REGISTER sip:%s@[remote_ip]:[remote_port] SIP/2.0
    Via: SIP/2.0/[transport] [local_ip]:[local_port];branch=[branch]
    From: <sip:%s@[local_ip]:[local_port]>;tag=[pid]SIPpTag00[call_number]
    To: <sip:%s@[remote_ip]:[remote_port]>
    Call-ID: [call_id]
    CSeq: 1 REGISTER
    Contact: sip:%s@[local_ip]:[local_port]
    Max-Forwards: 70
    Content-Length: [len]
    ]]>
  </send>

  <recv response="401"
      auth="true">
  </recv>''' % (service, service, service, service)

    s += '''
<send>
    <![CDATA[
    REGISTER sip:%s@[remote_ip]:[remote_port] SIP/2.0
    Via: SIP/2.0/[transport] [local_ip]:[local_port];branch=[branch]
    From: <sip:%s@[local_ip]:[local_port]>;tag=[pid]SIPpTag00[call_number]
    To: <sip:%s@[remote_ip]:[remote_port]>
    Call-ID: [call_id]
    CSeq: 2 REGISTER
    Contact: sip:%s@[local_ip]:[local_port]
    [authentication username=%s password=%s]
    Max-Forwards: 70
    Content-Length: [len]
    ]]>
  </send>
  
  <recv response = "200">
  </recv>

  <label id = "auth_done" />
 ''' % (service, service, service, service, username, password)

    s += '''
  <send>
    <![CDATA[
INVITE sip:%s@[remote_ip]:[remote_port] SIP/2.0
Via: SIP/2.0/[transport] [local_ip]:[local_port];branch=[branch]
From: <sip:%s@[local_ip]:[local_port]>;tag=[pid]SIPpTag00[call_number]
To: <sip:%s@[remote_ip]:[remote_port]> 
Call-ID: IN///[call_id]
CSeq: 1 INVITE
Contact: sip:%s@[local_ip]:[local_port]
Max-Forwards: 70
Content-Length: [len]
Content-Type: application/sdp

v=0
o=user1 53655765 2353687637 IN IP[local_ip_type] [local_ip]
s=-
c=IN IP[media_ip_type] [media_ip]
t=0 0
m=audio [media_port] RTP/AVP 0
a=rtpmap:0 PCMU/8000  
     ]]>
  </send>

  <recv response = "401"
        auth = "true">
  </recv>

''' % (number, service, service, service)

    s += '''
<send>
    <![CDATA[
INVITE sip:%s@[remote_ip]:[remote_port] SIP/2.0
Via: SIP/2.0/[transport] [local_ip]:[local_port];branch=[branch]
From: <sip:%s@[local_ip]:[local_port]>;tag=[pid]SIPpTag00[call_number]
To: <sip:%s@[remote_ip]:[remote_port]>
Call-ID: IN///[call_id]
CSeq: 2 INVITE
Contact: sip:%s@[local_ip]:[local_port]
[authentication username=%s password=%s]
Max-Forwards: 70
Content-Length: [len]
Content-Type: application/sdp

v=0
o=user1 53655765 2353687637 IN IP[local_ip_type] [local_ip]
s=-
c=IN IP[media_ip_type] [media_ip]
t=0 0
m=audio [media_port] RTP/AVP 0
a=rtpmap:0 PCMU/8000
    ]]>
  </send>

  <recv response="100"
        optional="true">
  </recv>

  <recv response="180" optional="true">
  </recv>

  <recv response="183" optional="true">
  </recv>

  <recv response="200" rtd="true">
  </recv>

''' % (number, service, service, service, username, password)

    s += '''
<send>
    <![CDATA[
    ACK sip:%s@[remote_ip]:[remote_port] SIP/2.0
    Via: SIP/2.0/[transport] [local_ip]:[local_port];branch=[branch]
    From: <sip:%s@[local_ip]:[local_port]>;tag=[pid]SIPpTag00[call_number]
    To: <sip:%s@[remote_ip]:[remote_port]>
    Call-ID: IN///[call_id]
    CSeq: 1 ACK
    Contact: sip:%s@[local_ip]:[local_port]
    Max-Forwards: 70
    Content-Length: [len]
    ]]>
  </send>

  <pause milliseconds = "5000"/>

    ''' % (service, service, service, service)

    s += '''
<send>
    <![CDATA[
    BYE sip:%s@[remote_ip]:[remote_port] SIP/2.0
    Via: SIP/2.0/[transport] [local_ip]:[local_port];branch=[branch]
    From: <sip:%s@[local_ip]:[local_port]>;tag=[pid]SIPpTag00[call_number]
    To: <sip:%s@[remote_ip]:[remote_port]>
    Call-ID: [call_id]
    CSeq: 1 BYE
    Contact: sip:%s@[local_ip]:[local_port]
    Max-Forwards: 70
    Content-Length: [len]
    ]]>
  </send>

  <recv response = "200" crlf="true">
  </recv>

''' % (service, service, service, service)

    s += '''
<send>
    <![CDATA[

    REGISTER sip:%s@[remote_ip]:[remote_port] SIP/2.0
    Via: SIP/2.0/[transport] [local_ip]:[local_port];branch=[branch]
    From: <sip:%s@[local_ip]:[local_port]>;tag=[pid]SIPpTag00[call_number]
    To: <sip:%s@[remote_ip]:[remote_port]>
    Call-ID: [call_id]
    CSeq: 3 REGISTER
    Expires: 0
    Contact: sip:%s@[local_ip]:[local_port]
    Max-Forwards: 70
    Content-Length: [len]
    ]]>
  </send>

<recv response = "401"
      auth = "true">
</recv>

''' % (service, service, service, service)

    s += '''

<send>
  <![CDATA[
REGISTER sip:%s@[remote_ip]:[remote_port] SIP/2.0
Via: SIP/2.0/[transport] [local_ip]:[local_port];branch=[branch]
From: <sip:%s@[local_ip]:[local_port]>;tag=[pid]SIPpTag00[call_number]
To: <sip:%s@[remote_ip]:[remote_port]>
Call-ID: [call_id]
CSeq: 4 REGISTER
Contact: sip:%s@[local_ip]:[local_port]
[authentication username=%s password=%s]
Expires: 0
Max-Forwards: 70
Content-Length: [len]
  ]]>
  
</send>

<recv response = "200">
</recv>

</scenario>
''' % (service, service, service, service, username, password)

    s = s.split('\n')
    s = [x.strip() for x in s]
    s = '\n'.join(s)
    return s
   
        
if __name__ == "__main__":
    main()
