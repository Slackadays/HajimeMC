#include <filesystem>
#include <stdlib.h>
#include <fstream>
#include <sys/mount.h>
#include <cstring>
#include <errno.h>
#include <iostream>
#include <string>
#include <unistd.h>

#include "output.h"

using std::shared_ptr;
using std::string;
using std::fstream;
using std::to_string;
using std::ofstream;
using std::ios;

namespace fs = std::filesystem;

class Server {
	bool hasOutput = false;
	bool hasOutputUSB = false; //if one variable is used, then the debug action will interfere wihh the USB mount function
	bool hasMounted = false;
	
	int systemi = 0;

	const string systems[7] = {"ext2", "ext3", "ext4", "vfat", "msdos", "f2fs", "fuseblk"};
	
	void mountDrive();
	void makeDir();
	void startProgram();
	void readSettings(string confFile);

	string file, path, command, confFile, device;
	
	std::shared_ptr<Output> fileObj;
	
	public:
		bool isRunning = false;
		void startServer(string confFile, shared_ptr<Output> fileObj);
		int getPID();
};

void Server::startServer(string confFile, shared_ptr<Output> tempObj) {
	
	fileObj = tempObj;
	
	try {
		if (fs::is_regular_file(confFile)) {
			fileObj->out("Reading settings...");
			readSettings(confFile);
		} else {
			fileObj->out("The server's config file doesn't exist");
			return;
		}
			fileObj->out("The file is: " + file);
			fileObj->out("The path is: " + path);
			fileObj->out("Command: " + command);
			//fileObj->out("Debug value: " + to_string(debug)); // ->out wants a string so we convert the debug int (converted from a string) back to a string
			fileObj->out("Device: " + device);
		
		while(true) {
			if (getPID() != 0) { //getPID looks for a particular keyword in /proc/PID/cmdline that signals the presence of a server
				sleep(3);
				fileObj->out("Program is running!");
				isRunning = true;
				hasMounted = true;
			} else {
					isRunning = false;
					fileObj->out("isRunning is now false");
			}
			fs::current_path(path);
			if (fs::current_path() == path && fs::is_regular_file(file) && !isRunning) { 			//checks if we're in the right place and if the server file is there
				fileObj->out("Trying to start program");
				startProgram();
				fileObj->out("Program start completed");
				
			}

			sleep(2);
			

			if (!fs::is_directory(path)) { 		//if the desired path doesn't exist, make it
				
				makeDir();
				
			}
			
			fs::current_path(path);
			
			if (!hasMounted) {
				
				mountDrive();
				
			}
		}
	} catch(string mes){
		fileObj->out(mes);
	} catch(...) { //error handling
		fileObj->out("Whoops! An error occurred.");
	}
}

void Server::startProgram() {
	
	if (!isRunning) {
		
		fileObj->out("Starting program!");
		
		fs::current_path(path);

		fs::remove("world/session.lock"); 		//session.lock will be there if the server didn't shut down properly

		//system() is a C command too
		system(command.c_str()); //execute the command
		
		sleep(1);
		
		if (getPID() != 0) { //check for the PID of the program we just started
			isRunning = true; //isRunning disables a lot of checks
			hasMounted = true;
		}
	}
}

void Server::makeDir() {
	fileObj->out("No directory!");
	if (!fs::create_directory(path)) {
		fileObj->out("Error creating directory!");
	}
}

void Server::mountDrive() {
	
	fileObj->out("Trying to mount.");
	
	if (!fs::is_empty(path)) { //if there are files, then we don't want to mount there
		
			fileObj->out("There are files in the path");
			return;
			
	} else {
		
		string error;
		
	if (mount(device.c_str(), path.c_str(), systems[systemi].c_str(), 0, "") == 0) { //brute-forces every possible filesystem because mount() depends on it being the right one
		
		fileObj->out("Device mounted!");
		hasMounted = true;
		systemi = 0; //reset in case it needs to mount again
		
	} else {
		int errsv = errno; //errno is the POSIX error code
		if (systemi == 6) {
			switch (errsv) {
			case 1 : error = "Not permitted. Is the device correct?"; break;
			case 2 : error = "No such file or directory."; break;
			case 13: error = "Permission denied. Is Hajime being run under root?"; break;
			case 5 : error = "Input/output error. Is the drive OK?"; break;
			case 12: error = "Not enough memory. Is there a shortage of it?"; break;
			case 11: error = "Resource unavailable."; break;
			case 14: error = "Bad address."; break;
			case 15: error = "Not a block device. Did you make sure you're mounting a mass storage device?"; break;
			case 16: error = "Busy. Is the device being accessed right now?"; break;
			case 21: error = "It's a directory. Did you make sure you're mounting a mass stoage device?"; break;
			case 22: error = "Bad arguments. Is the configuration set correctly?"; break;
			case 19: error = "Unknown device. Is the filesystem supported?"; break;
			default: error = "Unknown error.";
			}
			if (!hasOutputUSB){
				fileObj->out("An error occurred, but the script will keep trying to mount. Error: " + error);
				hasOutputUSB = true;
				systemi = 0;
			}
			fileObj->out("Error code: " + to_string(errsv));
			}
		}
		if (systemi < 6) {
			fileObj->out("Trying " + systems[systemi] + " filesystem");
			systemi++; //increment the filesystem
		}
	}
}

void Server::readSettings(string confFile) {
	
	std::fstream conf; 	//conjure up a file stream

	conf.open(confFile, std::fstream::in); //configuration file open for reading

	int iter = 0;
	int lineNum = 0;
	string var[6], param[6], line;
    string finished = "";
	//checks if there's stuff left to read
	while (conf.good() && lineNum < 6) { //linenum < 6 because otherwise, we get a segmentation fault
		getline(conf, line); //get a line and save it to line
		
		if (line == ""){
			throw "Whoops! The config file doesn't have anything in it.";
		}
		//if we've reachd the end of the config section (#) then get out of the loop!
		if (line[iter] == '#') {
					break;
		}
		param[lineNum] = "";
		//skips past anything that isn't in a quote
		//single quotes mean a char, and escape the double quote with a backslash
		while (line[iter] != '=') {
			param[lineNum] = param[lineNum] + line[iter];
			iter++;
		}
		//the current position is that of a quote, so increment it 1
		iter++;
		//append the finished product
		while ((uint)iter < line.length()) {
			finished = finished + line[iter];
			iter++;
		}

		//make the var[] what the finished product is
		var[lineNum] = finished;
		iter = 0; 		//reset for the next loop
		finished = "";
		if (param[lineNum] == "file") {file = var[lineNum];}
		if (param[lineNum] == "path") {path = var[lineNum];}
		if (param[lineNum] == "command") {command = var[lineNum];}
		//if (param[lineNum] == "debug") {debug = stoi(var[lineNum]);} //stoi() converts the string result into an int
		if (param[lineNum] == "device") {device = var[lineNum];}
		lineNum++; 		//prep var[] for the next line
	}
	
	if (device == "") {
		fileObj->out("No device requested: No mounting this time!");
		hasMounted = true;
	}

	conf.close(); //get rid of the file in memory
}

int Server::getPID() {
	string dir = "";
	fs::directory_iterator Directory("/proc/"); //search /proc/
	fs::directory_iterator End;					//a dummy object to compare to

	while (Directory != End) { 
		dir = Directory->path();				//assigns a formatted directory string to dir
		fstream file;							 //create a file object
		file.open(dir + "/cmdline", ios::in); 	//open the file of /proc/PID/cmdline for reading
		string str = ""; 						//reset string
		getline(file, str); 					//read cmdline (it is only 1 line)
		if (str.length() > 0){ 					//if a cmdline is not used, there will be nothing
			if (str.find("SCREEN") != string::npos){ //look for a keyword in cmdline, string::npos is a special value (-1) that needs to be used
				file.close(); //erase from memory
				return stoi(dir.erase(0, 6)); 	//return the PID of the known good process
					
			}
		}
		
	file.close(); //erase the file from memory
	
	Directory++; //look at the next directory
	}
	return 0; //0 = doesn't exist
}
