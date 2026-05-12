from flask import Flask, request, render_template, jsonify
import serial
import time

app = Flask(__name__)

# --- НАЛАШТУВАННЯ ПОРТУ ---
SERIAL_PORT = 'COM3'
BAUD_RATE = 9600

try:
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    print(f"Підключено до {SERIAL_PORT}")
except Exception as e:
    print(f"Помилка підключення до плати: {e}")
    ser = None


@app.route('/')
def index():
    # Відкриває ваш HTML файл
    return render_template('index.html')


@app.route('/api/send', methods=['POST'])
def send_command():
    global ser

    # 1. АВТО-ПЕРЕПІДКЛЮЧЕННЯ: Якщо плати немає, пробуємо її знайти
    if ser is None:
        try:
            ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
            print(f"Плата знайдена! Підключено до {SERIAL_PORT}")
        except Exception:
            # Якщо все ще не підключена - повертаємо 500
            return jsonify({"status": "error", "message": "Плата не підключена"}), 500

    data = request.json
    cmd = data.get('cmd')

    try:
        # Перевірка (пінг) від веб-інтерфейсу
        if cmd == '':
            _ = ser.in_waiting
            return jsonify({"status": "ping_ok"}), 200

        # Звичайна відправка команди
        if cmd:
            print(f"Відправка команди: {cmd.strip()}")
            ser.write(cmd.encode())  # Відправляємо символ у UART
            return jsonify({"status": "ok", "sent": cmd})

    except Exception as e:
        # Якщо під час роботи витягнули кабель
        print(f"Зв'язок втрачено: {e}")
        if ser:
            ser.close()
        ser = None  # Скидаємо змінну, щоб наступні рази працювало авто-перепідключення
        return jsonify({"status": "error", "message": "Зв'язок втрачено"}), 500

    return jsonify({"status": "fail"}), 400


if __name__ == '__main__':
    # Запуск сервера
    app.run(host='0.0.0.0', port=5000, debug=False)
