from flask import Flask, request, jsonify
from pymongo import MongoClient
import socket

app = Flask(__name__)

# MongoDB Atlas bağlantısı
from pymongo import MongoClient

client = MongoClient("mongodb+srv://cansuzerrr_db_user:DP4UtXBG4l8YZzFX@cluster0.7o3qyzz.mongodb.net/kablotestdatası?retryWrites=true&w=majority")
db = client['kablotestdatası']
collection = db['kablotestcollection']


@app.route('/data', methods=['POST'])
def receive_data():
    data = request.get_json()
    if not data:
        print("JSON alınamadı")
        return jsonify({"status": "error", "message": "No JSON received"}), 400
    print(f"Gelen JSON: {data}")
    try:
        collection.insert_one(data)
        return jsonify({"status": "success", "message": "Data saved!"})
    except Exception as e:
        print(f"MongoDB kaydı sırasında hata: {e}")
        return jsonify({"status": "error", "message": str(e)}), 500

def get_local_ip():
    """Bilgisayarın yerel IP adresini al"""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        # Herhangi bir dış IP’ye bağlanmaya çalışıyoruz
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
    except:
        ip = "127.0.0.1"
    finally:
        s.close()
    return ip

if __name__ == '__main__':
    local_ip = get_local_ip()
    port = 5000
    print(f"Server başlatılıyor... Erişim için URL: http://{local_ip}:{port}/data")
    app.run(host='0.0.0.0', port=port, debug=True)

    
