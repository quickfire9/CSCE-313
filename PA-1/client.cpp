/*
Original author of the starter code
	Tanzir Ahmed
	Department of Computer Science & Engineering
	Texas A&M University
	Date: 2/8/20

	Please include your Name, UIN, and the date below
	Name: Jeffrey Baker
	UIN:  934001568
	Date: 9-26-25
*/

#include "common.h"
#include "FIFORequestChannel.h"
#include <sys/wait.h>
#include <fstream>

using namespace std;


int main (int argc, char *argv[]) {
	int opt;
	int p = 0;
	double t = -1.0;
	int e = 0;
	int m_arg = 256;
	bool new_channel = false;

	string filename;
	while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
				break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'f':
				filename = optarg;
				break;
			case 'm':
				m_arg = atoi (optarg);
				break;
			case 'c':
				new_channel = true;
				break;
		}
	}

	pid_t server = fork();
	if (server < 0) {
		EXITONERROR("fork failed");
	}

	if (server == 0) {
		// child process
		std::string m_string = std::to_string(m_arg);
		execl("./server", "server", "-m", m_string.c_str(), (char *)NULL);
		EXITONERROR("execl failed!");
	}

    FIFORequestChannel control_chan("control", FIFORequestChannel::CLIENT_SIDE);
    FIFORequestChannel* chan = &control_chan; // Use a pointer to swap refrence to new channel if needed

    if (new_channel) {

        MESSAGE_TYPE m = NEWCHANNEL_MSG;
        control_chan.cwrite(&m, sizeof(MESSAGE_TYPE));

        char new_chan_name[256];
        control_chan.cread(new_chan_name, 256);

    	//set pointer to reference newly created channel
        chan = new FIFORequestChannel(new_chan_name, FIFORequestChannel::CLIENT_SIDE);
    }
    


	if (p > 0 && t >= 0.0 && 0 < e && e <= 2) {
		datamsg input(p, t, e);
		chan->cwrite(&input, sizeof(datamsg)); 
		double reply;
		chan->cread(&reply, sizeof(double));

		cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
	}
	else if (p > 0 && t == -1 && e == 0) {
		double time_it = 0;
		std::ofstream output("received/x1.csv");
		// Requesting the first 1000 data points
		for (int i=0; i < 1000; i++) {
			datamsg ecg1(p, time_it, 1);
			chan->cwrite(&ecg1, sizeof(datamsg)); 
			double reply1;
			chan->cread(&reply1, sizeof(double)); 

			datamsg ecg2(p, time_it, 2);
			chan->cwrite(&ecg2, sizeof(datamsg)); 
			double reply2;
			chan->cread(&reply2, sizeof(double)); 

			output << time_it << "," << reply1 << "," << reply2 << "\n";
			time_it += 0.004;
		}
        output.close();
	} else if (t == -1 && e == 0 && !filename.empty()){
		filemsg fm(0, 0);
		string fname = std::string(filename);
		int len = sizeof(filemsg) + (fname.size() + 1);
		char* buf2 = new char[len];
		memcpy(buf2, &fm, sizeof(filemsg));
		strcpy(buf2 + sizeof(filemsg), fname.c_str());
		chan->cwrite(buf2, len);
		delete[] buf2; 
		
		long long file_size;
		chan->cread(&file_size, sizeof(long long));

		cout << "File length is: " << file_size << " Bytes" << endl;

		char* received_buf = new char[m_arg];
		long long size_iterator = 0; 
		std::ofstream output_file("received/" + filename, std::ios::binary);
		
        while (size_iterator < file_size) {
			long long remaining_bytes = file_size - size_iterator;
			int length_to_request = min((long long)m_arg, remaining_bytes);

			filemsg chunk_request(size_iterator, length_to_request);
			int request_len = sizeof(filemsg) + fname.size() + 1;
			char* request_buf = new char[request_len];
			memcpy(request_buf, &chunk_request, sizeof(filemsg));
			strcpy(request_buf + sizeof(filemsg), fname.c_str());

			chan->cwrite(request_buf, request_len);
			delete[] request_buf;

			int bytes_read = chan->cread(received_buf, length_to_request);
			if (bytes_read > 0) {
				output_file.write(received_buf, bytes_read);
				size_iterator += bytes_read;
			} else {
				break;
			}
		}
		delete[] received_buf;
		output_file.close();
	}


    MESSAGE_TYPE m = QUIT_MSG;
    chan->cwrite(&m, sizeof(MESSAGE_TYPE));
    

    if (new_channel) { //quit initial control too incase used channel issnt pointing to control
        control_chan.cwrite(&m, sizeof(MESSAGE_TYPE));
        delete chan; //delete pointer
    }

	// wait(NULL); // broken?? ta told me to remove and then thing started working agian...

	return 0;
}