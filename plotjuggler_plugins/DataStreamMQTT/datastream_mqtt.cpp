#include "datastream_mqtt.h"
#include "ui_datastream_mqtt.h"
#include <QMessageBox>
#include <QSettings>
#include <QDebug>
#include <QUuid>


void connect_callback(struct mosquitto *mosq, void *context, int result)
{
  DataStreamMQTT* _this = static_cast<DataStreamMQTT*>(context);

  if( result != 0 )
  {
    qDebug() << QString("Problem connecting to MQTT: %1").arg(result);
    return;
  }

  mosquitto_subscribe(mosq, nullptr,_this->_topic_filter.toStdString().c_str(), _this->_qos);
}

void disconnect_callback(struct mosquitto *mosq, void *context, int result)
{
  DataStreamMQTT* _this = static_cast<DataStreamMQTT*>(context);
  qDebug() << "disconnect callback, rc = " << result;
}

void message_callback(struct mosquitto *mosq, void *context, const struct mosquitto_message *message)
{
  DataStreamMQTT* _this = static_cast<DataStreamMQTT*>(context);

  auto it = _this->_parsers.find(message->topic);
  if( it == _this->_parsers.end() )
  {
    auto parser = _this->availableParsers()->at( _this->_protocol )->createInstance({}, _this->dataMap());
    it = _this->_parsers.insert( {message->topic, parser} ).first;
  }
  auto& parser = it->second;

  try {
    MessageRef msg( static_cast<uint8_t*>(message->payload), message->payloadlen);

    using namespace std::chrono;
    auto ts = high_resolution_clock::now().time_since_epoch();
    double timestamp = 1e-6* double( duration_cast<microseconds>(ts).count() );

    parser->parseMessage(msg, timestamp);

  } catch (std::exception& ) {
//    _this->_protocol_issue = true;
    return;
  }
  emit _this->dataReceived();
}

/*
 *  mid -         the message id of the subscribe message.
 *  qos_count -   the number of granted subscriptions (size of granted_qos).
 *  granted_qos - an array of integers indicating the granted QoS for each of
 *                the subscriptions.
 */
void subscribe_callback(struct mosquitto *mosq, void *context, int mid, int qos_count, const int *granted_qos)
{
  DataStreamMQTT* _this = static_cast<DataStreamMQTT*>(context);
  _this->_subscribed = true;
}

void unsubscribe_callback(struct mosquitto *mosq, void *context, int result)
{
  DataStreamMQTT* _this = static_cast<DataStreamMQTT*>(context);
  qDebug() << QString("Subscription Failure. Code %1").arg(result);
  _this->_finished = true;
}



class MQTT_Dialog: public QDialog
{
public:
  MQTT_Dialog():
    QDialog(nullptr),
    ui(new Ui::DataStreamMQTT)
  {
    ui->setupUi(this);

    static QString uuid =  QString::number(rand());
    ui->lineEditClientID->setText(tr("Plotjuggler-") + uuid);
  }

  ~MQTT_Dialog() {
    while( ui->layoutOptions->count() > 0)
    {
      auto item = ui->layoutOptions->takeAt(0);
      item->widget()->setParent(nullptr);
    }
    delete ui;
  }

  Ui::DataStreamMQTT* ui;
};

//---------------------------------------------


DataStreamMQTT::DataStreamMQTT():
  _running(false)
{
  mosquitto_lib_init();
}

DataStreamMQTT::~DataStreamMQTT()
{
  shutdown();
  mosquitto_lib_cleanup();
}

bool DataStreamMQTT::start(QStringList *)
{
  if (_running)
  {
    return _running;
  }

  if( !availableParsers() )
  {
    QMessageBox::warning(nullptr,tr("MQTT Client"), tr("No available MessageParsers"),  QMessageBox::Ok);
    _running = false;
    return false;
  }

  MQTT_Dialog* dialog = new MQTT_Dialog();

  for( const auto& it: *availableParsers())
  {
    dialog->ui->comboBoxProtocol->addItem( it.first );

    if(auto widget = it.second->optionsWidget() )
    {
      widget->setVisible(false);
      dialog->ui->layoutOptions->addWidget( widget );
    }
  }


  std::shared_ptr<MessageParserCreator> parser_creator;

  connect(dialog->ui->comboBoxProtocol, qOverload<int>( &QComboBox::currentIndexChanged), this,
          [&](int index)
          {
            if( parser_creator ){
              if(auto prev_widget = parser_creator->optionsWidget() ){
                prev_widget->setVisible(false);
              }
            }
            QString protocol = dialog->ui->comboBoxProtocol->itemText(index);
            parser_creator = availableParsers()->at( protocol );

            if(auto widget = parser_creator->optionsWidget() ){
              widget->setVisible(true);
            }
          });

  // load previous values
  QSettings settings;
  QString address = settings.value("DataStreamMQTT::address").toString();
  _protocol = settings.value("DataStreamMQTT::protocol", "JSON").toString();
  _topic_filter = settings.value("DataStreamMQTT::filter").toString();
  _qos = settings.value("DataStreamMQTT::qos", 0).toInt();
  QString username = settings.value("DataStreamMQTT::username", "").toString();
  QString password = settings.value("DataStreamMQTT::password", "").toString();

  dialog->ui->lineEditAddress->setText( address );
  dialog->ui->comboBoxProtocol->setCurrentText(_protocol);
  dialog->ui->lineEditTopicFilter->setText( _topic_filter );
  dialog->ui->comboBoxQoS->setCurrentIndex(_qos);
  dialog->ui->lineEditUsername->setText(username);
  dialog->ui->lineEditPassword->setText(password);

  if( dialog->exec() == QDialog::Rejected )
  {
    return false;
  }

  address = dialog->ui->lineEditAddress->text();
  _protocol = dialog->ui->comboBoxProtocol->currentText();
  _topic_filter = dialog->ui->lineEditTopicFilter->text();
  _qos = dialog->ui->comboBoxQoS->currentIndex();
  QString cliend_id = dialog->ui->lineEditClientID->text();
  username = dialog->ui->lineEditUsername->text();
  password = dialog->ui->lineEditPassword->text();

  dialog->deleteLater();

  // save back to service
  settings.setValue("DataStreamMQTT::address", address);
  settings.setValue("DataStreamMQTT::filter", _topic_filter);
  settings.setValue("DataStreamMQTT::protocol", _protocol);
  settings.setValue("DataStreamMQTT::qos", _qos);
  settings.setValue("DataStreamMQTT::username", username);
  settings.setValue("DataStreamMQTT::password", password);

  _subscribed = false;
  _finished = false;
  _running = false;

  if(mosquitto_sub_topic_check(_topic_filter.toStdString().c_str()) == MOSQ_ERR_INVAL)
  {
    QMessageBox::warning(nullptr,tr("MQTT Client"),
                         tr("Error: Invalid subscription topic '%1', are all '+' and '#' wildcards correct?").arg(_topic_filter),
                         QMessageBox::Ok);
    return false;
  }

//  _protocol_issue = false;

  struct mosquitto *mosq = mosquitto_new(address.toStdString().c_str(), true, this);

  if(!mosq)
  {
    QMessageBox::warning(nullptr,tr("MQTT Client"),
                         tr("Problem creating the Mosquitto client"),  QMessageBox::Ok);
    _running = false;
    return false;
  }

  mosquitto_connect_callback_set(mosq, connect_callback);
  mosquitto_disconnect_callback_set(mosq, disconnect_callback);
  mosquitto_message_callback_set(mosq, message_callback);

  mosquitto_subscribe_callback_set(mosq, subscribe_callback);
  mosquitto_unsubscribe_callback_set(mosq, unsubscribe_callback);

  client_opts_set(mosq, &cfg)

  _running = true;
//  _protocol_issue_timer.start(500);

  return _running;
}

void DataStreamMQTT::shutdown()
{
  if( _running )
  {
    _disconnection_done = false;


    _running = false;
    _parsers.clear();
  }
}

bool DataStreamMQTT::isRunning() const
{
  return _running;
}



