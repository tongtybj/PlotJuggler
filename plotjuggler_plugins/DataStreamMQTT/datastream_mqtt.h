#ifndef DATASTREAM_MQTT_H
#define DATASTREAM_MQTT_H

#include <QDialog>
#include <QtPlugin>
#include <QTimer>
#include <thread>
#include "PlotJuggler/datastreamer_base.h"
#include "PlotJuggler/messageparser_base.h"
#include "ui_datastream_mqtt.h"

#include <mosquitto.h>

using namespace PJ;

struct MosquittoConfig {
  QString id;
  QString id_prefix;
  int protocol_version;
  int keepalive;
  QString host;
  int port;
  int qos;
  bool retain;
  QString bind_address;
#ifdef WITH_SRV
  bool use_srv;
#endif
  unsigned int max_inflight;
  QString username;
  QString password;
  QString will_topic;
  QString will_payload;
  long will_payloadlen;
  int will_qos;
  bool will_retain;
#ifdef WITH_TLS
  QString cafile;
  QString capath;
  QString certfile;
  QString keyfile;
  QString ciphers;
  bool insecure;
  QString tls_version;
#  ifdef WITH_TLS_PSK
  QString psk;
  QString psk_identity;
#  endif
#endif
  bool clean_session; /* sub */
  std::vector<QString> topics; /* sub */
  bool no_retain; /* sub */
  std::vector<QString> filter_outs; /* sub */
  int filter_out_count; /* sub */
  bool verbose; /* sub */
  bool eol; /* sub */
  int msg_count; /* sub */
#ifdef WITH_SOCKS
  QString socks5_host;
  int socks5_port;
  QString socks5_username;
  QString socks5_password;
#endif
};


class DataStreamMQTT : public PJ::DataStreamer
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "facontidavide.PlotJuggler3.DataStreamer")
  Q_INTERFACES(PJ::DataStreamer)

public:
  DataStreamMQTT();

  ~DataStreamMQTT() override;

  virtual bool start(QStringList*) override;

  virtual void shutdown() override;

  virtual bool isRunning() const override;

  virtual const char* name() const override
  {
    return "MQTT Subscriber (Mosquitto)";
  }

  virtual bool isDebugPlugin() override
  {
    return false;
  }

  bool _disconnection_done;
  bool _subscribed;
  bool _finished;
  bool _running;

  std::unordered_map<std::string, PJ::MessageParserPtr> _parsers;

  struct mosquitto *mosq;
  MosquittoConfig config;


private slots:


};


#endif // DATASTREAM_MQTT_H
