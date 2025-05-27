const mongoose = require('mongoose');
require('dotenv').config();

const connectDB = async () => {
  try {
    await mongoose.connect(process.env.MONGODB_URI, {
      useNewUrlParser: true,
      useUnifiedTopology: true,
    });
    console.log('MongoDB conectado!');
  } catch (err) {
    console.error('Erro na conexão MongoDB:', err.message);
    process.exit(1);
  }
};

module.exports = connectDB;