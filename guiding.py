##############################################################
#                                                            #
#                     guiding.py v2.0                        #
#                                                            #
#                      James McCormac                        #
#                      Paul Chote                            #
#                                                            #
##############################################################

##################################################
############## Libraries Required ################
##################################################

import time, os, os.path, sys
import glob as g
import argparse as ap
import datetime
import subprocess

##################################################
############### Global Variables #################
##################################################

# e.g. .fits or .fit etc
image_extension = "*.fits"

# guider log file name
logfile = "guider.log"

# wait time (s) - time to wait before ending
waittime = 8000

# data base directory
base_dir = "/home/paul/OBS_DATA/"

# max guide shift applied, pixels
max_correction = 15

# CCD region to guide on [xmin:xmax, ymin:ymax]
region = "[64:1984,64:1984]"

# Background sky tile size (should be much larger than the star FWHM)
tile_size = 64

##################################################
######### Load Command Line Arguments ############
##################################################

# get command line arguments
def ArgParse():
	parser = ap.ArgumentParser()
	parser.add_argument("--debug", help = "runs the script in DEBUGGING mode, no system calls made", action = "store_true")
	parser.add_argument("--v", help = "increased verbosity", action = "store_true")
	args = parser.parse_args()
	return args

args=ArgParse()

if args.debug:
	print "\n**********************"
	print "* DEBUGGING Mode ON! *"
	print "**********************\n"

##################################################
################## Functions #####################
##################################################

# apply guide corrections
def guide(x, y):
	print("Apply guide offset %f %f" % (x, y))
    print("TODO: Move the telescope")
	return 0

# log guide corrections
def LogShifts(logfile,ref,name,x,y):
	FILE = open(logfile, "a")
	line = "[%s] %s\t%s\t%.2f\t%.2f\n" % (datetime.datetime.utcnow().isoformat(), ref, name, x, y)
	FILE.write(line)
	FILE.close
	return 0

# get evening or morning
def GetAMorPM():
	return int(time.ctime().split()[3].split(':')[0]) < 12

# get tonights directory
def GetDataDir(tomorrow):
	is_am = GetAMorPM()
	if is_am > 0 and tomorrow > 0:
		tomorrow = 0

	d = datetime.date.today() - datetime.timedelta(days = is_am) + datetime.timedelta(days = tomorrow)	
	x = "%d%02d%02d" % (d.year, d.month, d.day)
	search_dir = "%s%s" % (base_dir, x)
	print "looking for %s" % (search_dir)
	if os.path.exists(search_dir):
		data_loc = search_dir
	else:
		data_loc = 1

	return data_loc

# wait for the newest image
# should work if there are none to begin with
def WaitForImage(field, tlim, imgs):
	timeout = 0
	while(1):
		t = g.glob(image_extension)

		if len(t) > imgs:
			timeout = 0
			# check for latest image - wait to be sure it's on disk
			time.sleep(1)

			# get newest image
			newest_img = max(t, key = os.path.getctime)

			# new start? if so, return the first new image
			if field == "":
				print "New start..."
				return newest_img

			# check that the field is the same
			if field != "" and field not in newest_img:
				print "New field detected..."
				return 999

			# check the field ad filters are the same
			if field != "" and field in newest_img:
				print "Same field, continuing..."
				return newest_img

		if len(t) == imgs:
			timeout = timeout + 1
			time.sleep(1)
			if timeout % 5 == 0:
				print "[%d/%d:%d - %s] No new images..." % (timeout, tlim, imgs, datetime.datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S'))

		if timeout > tlim:
			print "No new images in %d min, exiting..." % (tlim / 60.)
			return 1

##################################################
##################### Main #######################
##################################################

# used to determine when a night is classified as over
tomorrow = 0

# multiple day loop
while(1):
	# run in daemon mode, looking for tonight's directory
	data_loc = 1
	sleep_time = 10

	# loop while waiting on data directory to exist
	while(data_loc == 1):
		time.sleep(sleep_time)
		data_loc = GetDataDir(tomorrow)

		if data_loc != 1:
			print "Found data directory: %s" % (data_loc)
			os.chdir(data_loc)
			break

		print "[%s] No data directory yet..." % (datetime.datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S'))
		sleep_time = 120

	# if we get to here	we assume we have found the data directory
	# get a list of the images in the directory
	templist = g.glob(image_extension)

	# check for any data in there
	old_i = len(templist)

	# if no images for 10 hours quit
	if old_i == 0:
		ref_file = WaitForImage("", waittime, 0, "")
		if ref_file == 1:
			print "Quitting..."
			sys.exit(1)
	else:
		ref_file = max(templist, key = os.path.getctime)

	##################################################
	############## Scan for new images ###############
	##################################################

	# waiting for images loop (up to 1 hour then quit for that night)
	while (1):
		check_file = WaitForImage(ref_file.split('-')[0], waittime, old_i)
		if check_file == 1:
			print "Breaking back to first checks (i.e. tomorrow)"
			token = GetAMorPM()
			if token == 1:
				tomorrow=0
			else:
				tomorrow = 1
			break
		elif check_file == 999:
			print "New field detected, breaking back to s reference image creator"
			break

		if args.v:
			print "REF: %s CHECK: %s" % (ref_file, check_file)

		# Call pixelshift to calculate the required offset
		# Reverse the check and reference files so that it negates the offset (since we want to counteract the shift)
		try:
			offsets = subprocess.check_output(["/home/paul/pixelshift/pixelshift", check_file + region, ref_file + region, str(tile_size)], stderr=subprocess.STDOUT)
		except subprocess.CalledProcessError as e:
			print "Error calculating offset. Skipping correction. Error was:"
			print e.output
			break

		o = offsets.split(' ')
		offset_x = float(o[0])
		offset_y = float(o[1])

		if abs(offset_x) >= max_correction or abs(offset_y) >= max_correction:
			print "Predicted guide correction (%f, %f) > %d pixels!" % (offset_x, offset_y, max_correction)
			print "Skipping correction!"

		# give the guide shift if correction is within limits.
		if abs(offset_x) < max_correction and abs(offset_y) < max_correction and not args.debug:
			applied = guide(offset_x, offset_y)
			if applied > 0:
				print "Breaking back to first checks (i.e. tomorrow)"
				token = GetAMorPM()
				if token == 1:
					tomorrow = 0
				else:
					tomorrow = 1
				break

		if args.debug:
			print "[SIM] Guide correction Applied"

		log_it = LogShifts(logfile, ref_file, check_file, offset_x, offset_y)

		# reset the comparison templist so the nested while(1) loop
		# can find new images
		templist = g.glob(image_extension)
		old_i = len(templist)
