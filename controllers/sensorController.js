const SensorData = require('../models/SensorData');

exports.saveSensorData = async (req, res) => {
  try {
    const { temperature, humidity, ldrResistance, soilSensors, lightsStatus } = req.body;

    const newData = new SensorData({
      temperature,
      humidity,
      ldrResistance,
      soilSensors,
      lightsStatus
    });

    await newData.save();
    res.status(201).json({ message: 'Dados salvos com sucesso!' });

  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};