// o2oscservice.h -- support option for OSC I/O from o2host
//
// Roger B. Dannenberg
// Feb 2024

class Osc_io_info {
  public:
    string service;
    string ip;
    int port;
    bool is_tcp;

  Osc_io_info(string service_, string ip_, int port_, bool is_tcp_) {
      service = service_;
      ip = ip_;
      port = port_;
      is_tcp = is_tcp_;
  }
};

extern vector<Osc_io_info> from_osc_input;
extern vector<Osc_io_info> to_osc_output;

void insert_o2_to_osc();
void insert_osc_to_o2();
void insert_o2_to_osc_fields(int y, string service, string ip,
                             string port, string tcp);
void insert_osc_to_o2_fields(int y, string port, string tcp, string service);
void osc_input_initialize(const char *service_name, int port_num,
                          bool tcp_flag);
void osc_output_initialize(const char *service, const char *ip, int port_num,
                           bool tcp_flag);
void osc_input_output_finish();
