%% ETRS923 TP3 - Partie 2.a / 2.b : import du modèle ONNX dans MATLAB et inférence
% À exécuter dans MATLAB Online (ou MATLAB desktop) avec :
%   - Deep Learning Toolbox
%   - Deep Learning Toolbox Converter for ONNX Model Format (Add-Ons > Get Add-Ons > "ONNX")
% Fichiers nécessaires dans le dossier courant (sortis du notebook Colab) :
%   meteo_model.onnx, meteo_weights.json, meteo_test_vectors.csv

clear; clc;

%% 1. Import du réseau
% importNetworkFromONNX remplace importONNXNetwork (déconseillée depuis R2023b, vouée à disparaître).
% Syntaxe du sujet, encore acceptée sur les anciennes versions :
%   net = importONNXNetwork("meteo_model.onnx", OutputLayerType="classification");
net = importNetworkFromONNX("meteo_model.onnx");
disp(net)
summary(net)

%% 2. Paramètres de normalisation et noms de classes (générés par le notebook)
P = jsondecode(fileread("meteo_weights.json"));
classes = string(P.classes);              % ex. ["Clair" "Nuageux" "Pluie" "Neige"]
mu  = P.mean(:)';                         % 1 x N_IN
sig = P.std(:)';

%% 3. Une prédiction "à la main" (valeurs physiques comme celles de la carte)
% ordre des entrées : voir P.features (ex. temp °C, humidité %, pression mer hPa, dP 3h hPa)
x_phys = [8.0 92 1006.5 -2.1];
x_norm = (x_phys - mu) ./ sig;            % même normalisation qu'à l'entraînement
dlX = dlarray(single(x_norm), "BC");      % B = batch (1), C = caractéristiques (N_IN)

tic;
scores = predict(net, dlX);
t_ms = toc * 1000;
scores = extractdata(scores);
[p, k] = max(scores);
fprintf("Prédiction : %s (confiance %.1f %%) en %.2f ms\n", classes(k), 100*p, t_ms);
fprintf("  probabilités : %s\n", join(compose("%s=%.3f", classes(:), scores(:)), ", "));

%% 4. Validation : les sorties MATLAB doivent être identiques à celles de Python
T = readtable("meteo_test_vectors.csv", "TextType", "string");
nIn = numel(mu);  nOut = numel(classes);
ecart_max = 0;
for i = 1:height(T)
    xi = T{i, 1:nIn};
    s  = extractdata(predict(net, dlarray(single((xi - mu) ./ sig), "BC")));
    ref = T{i, nIn+1:nIn+nOut};
    ecart_max = max(ecart_max, max(abs(double(s(:)') - ref)));
    [~, ki] = max(s);
    fprintf("test %d : MATLAB=%-10s attendu=%-10s\n", i, classes(ki), T.classe_attendue(i));
end
fprintf("Écart max MATLAB / Python : %.2e  (OK si < 1e-4)\n", ecart_max);

%% 5. Temps d'inférence moyen (pour le comparatif Cloud / Edge)
N = 1000; tic;
for i = 1:N
    predict(net, dlX);
end
fprintf("Inférence MATLAB moyenne : %.1f µs\n", toc / N * 1e6);

% Optionnel : sauvegarder le réseau importé pour le réutiliser ailleurs (ex. ThingSpeak avec licence)
% save meteo_net.mat net
