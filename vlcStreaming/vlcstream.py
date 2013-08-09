
import sys
import os
import telnetlib

##======================================================================================================

if(len(sys.argv) < 5):
	print '\nERROR! usage \"python vlcstream.py host_ip streaming_port num_of_streams path_to_video\"\n'
	print '\nor     usage \"python vlcstream.py host_ip streaming_port num_of_streams path_to_video_dir\"\n'
	exit()


hostIp = sys.argv[1]

streamingPort = int(sys.argv[2])

numOfStreams = int(sys.argv[3])

path = sys.argv[4]
	
if(len(path) == 0):
	print '\nERROR! path to video is undefined\n'
	exit()

tn = telnetlib.Telnet("localhost", 4212)

tn.read_until("Password: ")
tn.write("123\n")
tn.read_until("> ")

if (os.path.isdir(path) == True):
	# play all videos from directory

	listVideo = [name for name in os.listdir(path) if name.endswith(".mp4")]
	
	if(len(listVideo) == 0):
		print '\nERROR! there is no video file in path dir\n'
		tn.write("quit\n")
		exit()

	listAbsVideo = []
	for video in listVideo:
		listAbsVideo.append(os.path.join(path,video))

	amountVideo = len(listAbsVideo)
	j = 0

	for i in xrange(numOfStreams):
		channel = 'channel' + str(i)
		portRtp = 1234 + i*2
		endpoint = hostIp + ':' + str(streamingPort)
		tn.write ('new ' + channel + ' broadcast enabled\n')
		tn.read_until ('> ')
		tn.write ('setup ' + channel + ' input ' + listAbsVideo[j] + '\n')
		print 'set ' + listAbsVideo[j]
		tn.read_until ('> ')
		tn.write ('setup ' + channel + ' output #rtp{dst=' + hostIp + ',port=' + \
			str(portRtp) + ',sdp=rtsp://' + endpoint + '/test' + str(i) + '.sdp}\n')
		tn.read_until ('> ')
		tn.write ('setup ' + channel + ' loop\n')
		tn.read_until ('> ')
		tn.write ('control ' + channel + ' play\n')
		tn.read_until ('> ')

		j += 1
		if(j == amountVideo):
			j = 0
else:
	for i in xrange(numOfStreams):
		channel = 'channel' + str(i)
		portRtp = 1234 + i*2
		endpoint = hostIp + ':' + str(streamingPort)
		tn.write ('new ' + channel + ' broadcast enabled\n')
		tn.read_until ('> ')
		tn.write ('setup ' + channel + ' input ' + path + '\n')
		tn.read_until ('> ')
		tn.write ('setup ' + channel + ' output #rtp{dst=' + hostIp + ',port=' + \
			str(portRtp) + ',sdp=rtsp://' + endpoint + '/test' + str(i) + '.sdp}\n')
		tn.read_until ('> ')
		tn.write ('setup ' + channel + ' loop\n')
		tn.read_until ('> ')
		tn.write ('control ' + channel + ' play\n')
		tn.read_until ('> ')


tn.write("quit\n")