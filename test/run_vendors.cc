#include <string>
#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <memory>

extern void run_server(const std::string, const std::string);

void run_vendors(const std::vector<std::pair<std::string, std::string> >& ip_addrresses);

std::pair<std::string, std::string> splitBy(const std::string& str, const std::string& delim) {
  std::pair<std::string, std::string> ret;
  size_t pos = str.find(delim);
  ret.first = str.substr(0, pos);
  pos += delim.size();
  ret.second = str.substr(pos);
  return ret;
}

int main(int argc, char** argv) {

  int addr_index = -1;
  std::string filename;
  if (argc == 3) {
    filename = std::string(argv[1]);
    addr_index = std::max( -1, atoi(argv[2]));
  }
  else if (argc == 2) {
    filename = std::string(argv[1]);
  }
  else {
    std::cerr << "Correct usage: ./run_vendors $file_path_for_server_addrress [$index]" << std::endl;
    return EXIT_FAILURE;
  }

  std::vector<std::pair<std::string, std::string> > ip_addrresses;
  // first = ip_addr
  // second = config file for this vendor

  std::ifstream myfile (filename);
  if (myfile.is_open()) {
    std::string ip_addr;
    while (getline(myfile, ip_addr)) {
      if (addr_index == -1) {
        ip_addrresses.push_back(splitBy(ip_addr, ";"));
      }
      else if (addr_index == 0) {
        ip_addrresses.push_back(splitBy(ip_addr, ";"));
        break;
      }
      else {
        --addr_index;
      }

    }
    myfile.close();
  }
  else {
    std::cerr << "Failed to open file " << filename << std::endl;
    return EXIT_FAILURE;
  }

  run_vendors(ip_addrresses);
  return EXIT_SUCCESS;
}


void run_vendors(const std::vector<std::pair<std::string, std::string> >& ip_addrresses) {

  typedef std::unique_ptr<std::thread> ThreadPtr;
  ThreadPtr threads[ip_addrresses.size()];

  for (int i = 0; i < ip_addrresses.size(); ++i) {
    threads[i] = ThreadPtr(new std::thread(run_server, ip_addrresses[i].first, ip_addrresses[i].second));
  }

  for (int i = 0; i < sizeof(threads); ++i) {
    threads[i]->join();
  }
}

