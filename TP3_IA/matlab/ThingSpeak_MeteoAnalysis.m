%% ETRS923 TP3 - Partie 2.c / 2.d : inférence Cloud dans ThingSpeak (app MATLAB Analysis)
% Lit les dernières mesures de la carte STM32 (canal du TP2), calcule les entrées du modèle,
% exécute le réseau de neurones, écrit le résultat dans un canal dédié et le pousse
% vers la carte via TalkBack.
% Déclenchement : app React "à chaque insertion de données" sur le canal du TP2
% (ou TimeControl, période minimale 5 min).

%% ===================== CONFIGURATION (à adapter) =====================
CANAL_MESURES   = 0000000;            % ID du canal du TP2
CLE_LECTURE     = 'XXXXXXXXXXXXXXXX'; % Read API Key du canal TP2 (vide si canal public)
CHAMP_TEMP      = 1;                  % numéros de champs utilisés au TP2
CHAMP_HUM       = 2;
CHAMP_PRES      = 3;                  % pression MESURÉE par le LPS22HH (hPa, à l'altitude de la carte)

CANAL_RESULTAT  = 0000000;            % nouveau canal "Meteo IA Cloud"
CLE_ECRITURE    = 'XXXXXXXXXXXXXXXX'; % Write API Key de ce canal

TALKBACK_ID     = 00000;              % Apps > TalkBack > ID
TALKBACK_CLE    = 'XXXXXXXXXXXXXXXX'; % clé API du TalkBack

ALTITUDE_M      = 235;                % campus du Bourget-du-Lac ≈ 235 m (+3 m par étage), voir guide
OFFSET_PRES_HPA = 0;                  % correction éventuelle du capteur (voir guide, §calibration)

%% ===================== POIDS DU RÉSEAU =====================
% Coller ici le contenu du fichier meteo_weights_inline.m généré par le notebook Colab.
% (il définit P.features, P.classes, P.mean, P.std, P.W{k}, P.b{k})
P = struct();
% ---- début du collage ----

% ---- fin du collage ----
if ~isfield(P, 'W')
    error('Poids absents : coller meteo_weights_inline.m dans la section POIDS.');
end

%% 1. Lecture des 200 dernières minutes de mesures
t_debut = tic;
[d, t] = thingSpeakRead(CANAL_MESURES, 'Fields', [CHAMP_TEMP CHAMP_HUM CHAMP_PRES], ...
                        'NumMinutes', 200, 'ReadKey', CLE_LECTURE);
ok = all(~isnan(d), 2);
d = d(ok, :);  t = t(ok);
if isempty(d)
    error('Aucune mesure exploitable dans les 200 dernières minutes.');
end

%% 2. Construction des entrées (mêmes calculs que sur le STM32)
% 2.a pression ramenée au niveau de la mer (formule du nivellement barométrique)
temp_c = d(:, 1);
p_mer  = (d(:, 3) + OFFSET_PRES_HPA) .* (1 - 0.0065*ALTITUDE_M ./ (temp_c + 0.0065*ALTITUDE_M + 273.15)).^(-5.257);

% 2.b dernière mesure (moyenne des 5 dernières minutes pour lisser le bruit)
recent = t >= t(end) - minutes(5);
temp = mean(d(recent, 1));
hum  = mean(d(recent, 2));
pres = mean(p_mer(recent));

% 2.c tendance de pression sur 3 h : mesure la plus proche de (maintenant - 3 h)
[ecart, i3h] = min(abs(t - (t(end) - hours(3))));
if ecart <= minutes(15)
    dp3h = pres - p_mer(i3h);
else
    dp3h = 0;                     % historique insuffisant : valeur neutre
    fprintf('Attention : moins de 3 h d''historique, dp3h forcé à 0\n');
end

% 2.d vecteur d'entrée dans l'ordre exact de P.features
dispo = struct('temp', temp, 'rhum', hum, 'pres', pres, 'dp3h', dp3h);
x = zeros(numel(P.features), 1);
for i = 1:numel(P.features)
    x(i) = dispo.(P.features{i});
end

%% 3. Inférence : réseau dense ReLU + softmax (identique au modèle ONNX)
t_inf = tic;
h = (x - P.mean) ./ P.std;                 % normalisation z-score
nCouches = numel(P.W);
for k = 1:nCouches
    h = P.W{k} * h + P.b{k};               % couche dense : y = W x + b
    if k < nCouches
        h = max(h, 0);                     % ReLU
    end
end
proba = exp(h - max(h));  proba = proba / sum(proba);   % softmax
[conf, classe] = max(proba);
duree_inference_ms = toc(t_inf) * 1000;

if isempty(t(end).TimeZone)
    maintenant = datetime('now');
else
    maintenant = datetime('now', 'TimeZone', t(end).TimeZone);
end
age_donnee_s = seconds(maintenant - t(end));   % si ~3600 s ou négatif : décalage de fuseau horaire
fprintf('Entrées : T=%.1f°C HR=%.0f%% P=%.1f hPa dP3h=%+.1f hPa\n', temp, hum, pres, dp3h);
fprintf('Résultat : %s (%.0f %%) | inférence %.3f ms | âge de la mesure %.0f s\n', ...
        P.classes{classe}, 100*conf, duree_inference_ms, age_donnee_s);

%% 4. Écriture du résultat dans le canal dédié (2.c)
% Field1 = classe (0..K-1, même numérotation qu'en C), Field2 = confiance %,
% Field3 = temps d'inférence (ms), Field4 = âge de la mesure (s), Field5..8 = probabilités
valeurs = [classe-1, round(100*conf), duree_inference_ms, age_donnee_s];
nProba  = min(4, numel(proba));
valeurs = [valeurs, round(100*proba(1:nProba))'];
thingSpeakWrite(CANAL_RESULTAT, valeurs, 'Fields', 1:numel(valeurs), 'WriteKey', CLE_ECRITURE);

%% 5. Commande TalkBack pour la carte (2.d)
% On vide d'abord la file : la carte ne doit lire que le résultat le plus récent.
url = sprintf('https://api.thingspeak.com/talkbacks/%d/commands', TALKBACK_ID);
try
    webread([url '.json'], 'api_key', TALKBACK_CLE, weboptions('RequestMethod', 'delete'));
catch err
    fprintf('Vidage TalkBack impossible : %s\n', err.message);
end
commande = sprintf('METEO:%d:%d', classe-1, round(100*conf));   % ex. METEO:2:87
webwrite([url '.json'], 'api_key', TALKBACK_CLE, 'command_string', commande);
fprintf('TalkBack : %s | durée totale du script %.2f s\n', commande, toc(t_debut));
