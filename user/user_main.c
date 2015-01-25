#include "user_interface.h"
#include "ets_sys.h"
#include "osapi.h"
#include "mem.h"
#include "espconn.h"
#include "driver/uart.h"

char initialized;
struct espconn net_conn;
esp_udp net_udp;
uint8_t data_buffer[BUF_COUNT][BUF_SIZE];
os_event_t sender_queue[SENDER_QUEUE_LEN];
static volatile os_timer_t timer;

/* Pop one Uart character and process it
   this method is directly called from driver/uart.c */
void ICACHE_FLASH_ATTR process_uart_char(void){
  static uint32_t buf_count;
  static uint32_t buf_next_char;
  uint8_t temp;
  uint8_t active_buf_num = buf_count % BUF_COUNT;

  temp = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;

  data_buffer[active_buf_num][buf_next_char++] = temp;
  if (buf_next_char >= BUF_SIZE){
    buf_count++;
    buf_next_char = 0;
    // Priority, Type, Message Content
    // Send signal to packet_sender thread
    system_os_post(TASK_PRIO_0, 0, active_buf_num);
  }
}

static void ICACHE_FLASH_ATTR wait_for_ip(uint8 flag){
  unsigned char ret = 0;
  char temp[20];
  struct ip_info ipconfig;
  os_timer_disarm(&timer);

  ret = wifi_station_get_connect_status();
  uart0_sendStr(".");

  if (ret != STATION_GOT_IP){
    // Wait and check again
    os_timer_setfn(&timer, (os_timer_func_t *)wait_for_ip);
    os_timer_arm(&timer, 100, 0);
  }else{
    wifi_get_ip_info(STATION_IF, &ipconfig);
    os_sprintf(temp, "%d.%d.%d.%d\r\n", IP2STR(&ipconfig.ip));
    uart0_sendStr("IP Address: ");
    uart0_sendStr(temp);

    // Create the UDP connection
    {
      unsigned long ip = 0;
      net_conn.type = ESPCONN_UDP;
      net_conn.state = ESPCONN_NONE;
      net_conn.proto.udp = &net_udp;
      net_conn.proto.udp->local_port = espconn_port();
      uart0_sendStr("Port: ");
      os_sprintf(temp, "%d\r\n", net_conn.proto.udp->local_port);
      uart0_sendStr(temp);
      net_conn.proto.udp->remote_port = SERVER_PORT;
      ip = ipaddr_addr(SERVER_IP);
      os_memcpy(net_conn.proto.udp->remote_ip, &ip, 4);
  
      ret = espconn_create(&net_conn);
    }
  }
  initialized = 1;
}

static void ICACHE_FLASH_ATTR system_init_done(void){
  // Connect to Wifi Station
  uart0_sendStr("System Initialized");
  wifi_station_connect();

  // Initialize the timer that checks for connection completion
  os_timer_disarm(&timer);
  os_timer_setfn(&timer, (os_timer_func_t *)wait_for_ip);
  os_timer_arm(&timer, 100, 0);
}

static void ICACHE_FLASH_ATTR packet_sender(os_event_t *e){
  // data_buffer is used to pass data
  // Get the buffer number
  char ret;
  uint8_t active_buf_num = e->par;
  uint8_t *active_buf = data_buffer[active_buf_num];

  // If network's not initialized, ignore packets
  if (!initialized){
    return;
  }

  // Send the data
  ret = espconn_sent(&net_conn, active_buf, BUF_SIZE);
  if (ret != ESPCONN_OK){
    uart0_sendStr("Error Sending UDP Packet, restarting\r\n");
    system_restart();
  }
}

void ICACHE_FLASH_ATTR user_init(void){
  initialized = 0;
  char ret = 0;
  struct station_config sconf;

  // Initialize UART
  uart_init(BIT_RATE_2400, BIT_RATE_2400);
  uart0_sendStr("ESP8266 Serial Device Ready\r\n");

  // Check the opmode
  ret = wifi_get_opmode();
  if (ret != STATION_MODE){
    // Wifi Operation Mode is STATION, which is set by default
    wifi_set_opmode(STATION_MODE);

    // Restart
    system_restart();
  }

  // Clear esp connection variables
  memset(&net_conn, 0, sizeof(net_conn));
  memset(&net_udp, 0, sizeof(net_udp));

  // Initialize AP values
  strcpy((char *)sconf.ssid, AP_SSID);
  strcpy((char *)sconf.password, AP_PASSWORD);
  uart0_sendStr("Connecting to: ");
  uart0_sendStr(sconf.ssid);
  uart0_sendStr(" (");
  uart0_sendStr(sconf.password);
  uart0_sendStr(")\r\n");

  // Set the Wifi Station Config
  ETS_UART_INTR_DISABLE();
  wifi_station_set_config(&sconf);
  ETS_UART_INTR_ENABLE();

  // Initialize the system init done callback
  // Wifi cannot connect until init is done
  system_init_done_cb(&system_init_done);

  // Start the UDP sending task
  system_os_task(packet_sender, TASK_PRIO_0, sender_queue, SENDER_QUEUE_LEN);
}
