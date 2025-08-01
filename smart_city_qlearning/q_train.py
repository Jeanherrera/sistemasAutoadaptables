import numpy as np
import pickle

n_states = 2**8  # 256 combinaciones posibles de los 8 sensores
n_actions = 4  # 0: Línea 1, 1: Línea 2, 2: No Vehículos, 3: Cruce Peatonal

q_table = np.zeros((n_states, n_actions))

episodes = 5000
alpha = 0.1
gamma = 0.9
epsilon = 0.2


def calculate_state(veh_c1, veh_c2, co2, vlS1, vlS2, other_city, mode, peatonal):
    return (
        (veh_c1 << 7)
        | (veh_c2 << 6)
        | (co2 << 5)
        | (vlS1 << 4)
        | (vlS2 << 3)
        | (other_city << 2)
        | (mode << 1)
        | (peatonal)
    )


for episode in range(episodes):
    # Simulación de entradas binarias aleatorias
    veh_c1 = np.random.choice([0, 1])
    veh_c2 = np.random.choice([0, 1, 2])  # puede haber más vehículos
    co2 = np.random.choice([0, 1])
    vlS1 = np.random.choice([0, 1])
    vlS2 = np.random.choice([0, 1])
    other_city = np.random.choice([0, 1])
    mode = np.random.choice([0, 1])  # 0: día, 1: noche
    peatonal = np.random.choice([0, 1])

    state = calculate_state(
        int(veh_c1 > 0), int(veh_c2 > 0), co2, vlS1, vlS2, other_city, mode, peatonal
    )

    # Elegir acción
    if np.random.rand() < epsilon:
        action = np.random.choice([0, 1, 2, 3])
    else:
        action = np.argmax(q_table[state])

    # Cálculo de recompensa
    reward = -1

    if peatonal == 1:
        reward = 10 if action == 3 else -5

    elif mode == 1:  # modo noche
        if veh_c1 == 0 and veh_c2 == 0:
            reward = 10 if action == 2 else -3
        elif veh_c1 > 0 and veh_c2 > 0:
            if veh_c1 > veh_c2:
                reward = 10 if action == 0 else -3
            else:
                reward = 10 if action == 1 else -3
        elif veh_c1 > 0:
            reward = 10 if action == 0 else -3
        elif veh_c2 > 0:
            reward = 10 if action == 1 else -3

    else:  # modo día
        if co2 == 1 or vlS2 == 1 or other_city == 1:
            reward = 10 if action == 1 else -5
        elif vlS1 == 1:
            reward = 10 if action == 0 else -5
        else:
            reward = 5 if action == 0 else -2  # comportamiento neutro

    # Actualizar tabla Q
    q_table[state, action] += alpha * (
        reward + gamma * np.max(q_table[state]) - q_table[state, action]
    )

# Guardar la tabla entrenada
with open("q_table.pkl", "wb") as f:
    pickle.dump(q_table, f)

print("Entrenamiento finalizado con reglas actualizadas.")
