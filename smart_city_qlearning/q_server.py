from flask import Flask, request, jsonify, render_template_string
import pickle
import numpy as np

app = Flask(__name__)

# Cargar la tabla entrenada
with open('q_table.pkl', 'rb') as f:
    q_table = pickle.load(f)

def calculate_state(veh_c1, veh_c2, co2, vlS1, vlS2, other_city, mode, peatonal):
    """Convierte los 8 valores a un entero entre 0 y 255"""
    return (
        (veh_c1 << 7) |
        (veh_c2 << 6) |
        (co2 << 5) |
        (vlS1 << 4) |
        (vlS2 << 3) |
        (other_city << 2) |
        (mode << 1) |
        (peatonal)
    )

@app.route('/decidir', methods=['POST'])
def decidir():
    data = request.get_json()

    # Convertir a 1 o 0 para que coincida con los datos entrenados
    veh_c1 = 1 if data.get('veh_c1', 0) > 0 else 0
    veh_c2 = 1 if data.get('veh_c2', 0) > 0 else 0
    co2 = 1 if data.get('co2', 0) > 2000 else 0
    vlS1 = 1 if data.get('vlS1', 1000) < 600 else 0
    vlS2 = 1 if data.get('vlS2', 1000) < 600 else 0
    other_city = int(data.get('sensor_other_city', 0))
    mode = int(data.get('currentMode', 0))  # 0: día, 1: noche
    peatonal = int(data.get('peatonalRequested', 0))

    state = calculate_state(veh_c1, veh_c2, co2, vlS1, vlS2, other_city, mode, peatonal)

    if state >= len(q_table):
        return jsonify({"error": "Estado fuera de la tabla Q", "state": state}), 400

    action = int(np.argmax(q_table[state]))
    return jsonify({"action": action})

modo_actual = {"modo": 0}

HTML_TEMPLATE = """
<!DOCTYPE html>
<html>
<head>
    <title>Selector de Modo</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; padding-top: 50px; }
        h1 { color: #333; }
        button { padding: 10px 20px; margin: 10px; font-size: 16px; cursor: pointer; }
        #modo { font-size: 24px; margin-top: 20px; }
    </style>
</head>
<body>
    <h1>Selector de Modo</h1>
    <button onclick="cambiarModo(0)">Modo Día (0)</button>
    <button onclick="cambiarModo(1)">Modo Noche (1)</button>
    <div id="modo">Modo actual: <strong id="estado-modo">Cargando...</strong></div>

    <script>
        async function obtenerModo() {
            const res = await fetch('/modo');
            const data = await res.json();
            document.getElementById('estado-modo').textContent = data.modo === 1 ? 'Noche (1)' : 'Día (0)';
        }

        async function cambiarModo(modo) {
            await fetch('/modo', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({modo})
            });
            obtenerModo();
        }

        // Obtener modo actual al cargar
        obtenerModo();
    </script>
</body>
</html>
"""

@app.route('/')
def interfaz_web():
    return render_template_string(HTML_TEMPLATE)

@app.route('/modo', methods=['GET'])
def obtener_modo():
    return jsonify(modo_actual)

@app.route('/modo', methods=['POST'])
def cambiar_modo():
    data = request.get_json()
    if not data or 'modo' not in data:
        return jsonify({"error": "Debe proporcionar el valor 'modo'"}), 400

    nuevo_modo = data['modo']
    if nuevo_modo not in [0, 1]:
        return jsonify({"error": "El modo debe ser 0 (día) o 1 (noche)"}), 400

    modo_actual["modo"] = nuevo_modo
    return jsonify({"mensaje": "Modo actualizado correctamente", "modo": nuevo_modo})



if __name__ == '__main__':
    app.run(host="0.0.0.0", port=5000)
