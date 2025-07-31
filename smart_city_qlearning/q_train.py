import numpy as np
import pickle

n_states = 8  # 2 x 2 x 2 combinaciones
n_actions = 2  # 0: Carril 1, 1: Carril 2

q_table = np.zeros((n_states, n_actions))

episodes = 1000
alpha = 0.1
gamma = 0.9
epsilon = 0.2

def calculate_state(co2, vis, veh):  # Todo binario
    return co2 * 4 + vis * 2 + veh  # Binary to decimal

for episode in range(episodes):
    co2 = np.random.choice([0, 1])
    vis = np.random.choice([0, 1])
    veh = np.random.choice([0, 1])
    
    state = calculate_state(co2, vis, veh)

    # Elegir acción
    if np.random.rand() < epsilon:
        action = np.random.choice([0, 1])
    else:
        action = np.argmax(q_table[state])

    # Definir recompensa
    if co2 == 1 or vis == 1 or veh == 1:
        reward = 10 if action == 1 else -5  # Prioriza Carril 2
    else:
        reward = 10 if action == 0 else -2  # Prioriza Carril 1

    # Actualización Q
    q_table[state, action] += alpha * (
        reward + gamma * np.max(q_table[state]) - q_table[state, action]
    )

# Guardar tabla
with open("q_table.pkl", "wb") as f:
    pickle.dump(q_table, f)

print("Entrenamiento terminado con sensores reales.")
