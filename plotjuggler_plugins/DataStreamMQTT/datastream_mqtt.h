#ifndef DATASTREAM_MQTT_H
#define DATASTREAM_MQTT_H

#include <QDialog>
#include <QtPlugin>
#include <QTimer>
#include <thread>
#include "PlotJuggler/datastreamer_base.h"
#include "PlotJuggler/messageparser_base.h"

#include <mosquitto.h>

using namespace PJ;

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

  QString _protocol;
  QString _topic_filter;
  int _qos;
  std::unordered_map<std::string, PJ::MessageParserPtr> _parsers;

private slots:

};


#endif // DATASTREAM_MQTT_H
