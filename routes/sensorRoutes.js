const express = require('express');
const router = express.Router();
const { saveSensorData } = require('../controllers/sensorController');

router.post('/api/data', saveSensorData);

module.exports = router;