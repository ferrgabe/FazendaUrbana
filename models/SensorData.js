// Configuração das tables que eu preciso no BD.

const mongoose = require('mongoose');

const SensorDataSchema = new mongoose.Schema({
  timestamp: { type: Date, default: Date.now },
  temperature: { type: Number, required: true }, // DHT22
  humidity: { type: Number, required: true },    // DHT22
  ldrResistance: { type: Number, required: true }, // LDR (kΩ)
  soilSensors: [{
    sensorId: { type: Number, required: true }, // ID do sensor FC-28 (0-51)
    moisture: { type: Number, required: true }, // Umidade do solo (%)
    activationCount: { type: Number, default: 0 } // Contagem de ativações da válvula
  }],
  lightsStatus: { type: Boolean, default: false } // Status do SSR (LDR)
});

module.exports = mongoose.model('SensorData', SensorDataSchema);