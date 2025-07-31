from flask import Flask, request, jsonify
import pickle
import numpy as np

app = Flask(__name__)

# Cargar tabla entrenada
with open('q_table.pkl', 'rb') as f:
    q_table = pickle.load(f)

@app.route('/')
def index():
    return 'Servidor de la Smart City funcionando correctamente.'

@app.route('/decidir', methods=['POST'])
def decidir():
    data = request.get_json()
    co2 = 1 if data['co2'] > 700 else 0
    vis = 1 if data['vis'] < 600 else 0
    veh_c1 = data['veh_c1']
    veh_c2 = data['veh_c2']
    veh = 1 if veh_c2 > veh_c1 else 0

    state = co2 * 4 + vis * 2 + veh
    action = int(np.argmax(q_table[state]))

    return jsonify({"action": action})

if __name__ == '__main__':
    app.run(host="0.0.0.0", port=5000)
