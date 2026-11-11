% TP3 ETRS923 - §2.c et §2.d : classification dans le Cloud MathWorks.
%
% A coller dans l'application "MATLAB Analysis" de ThingSpeak, declenchee
% par une application "React" a chaque nouvelle entree du canal de mesures
% (ou par un "TimeControl", au plus vite toutes les 5 minutes en gratuit).
%
% §2.c : lit les donnees reelles envoyees par la carte STM32, les classe, et
%        sauvegarde le resultat sur un canal specifique.
% §2.d : depose le resultat dans la file Talkback, ou la carte vient le lire
%        pour l'afficher.
%
% Canal de MESURES, rempli par la carte (Core/Src/cloud_thingspeak.c) :
%   field1 temperature degC | field2 humidite %RH | field3 pression hPa
%   field4 indice de la classe predite par le modele embarque
%
% Canal de RESULTATS, rempli par ce script :
%   field1 indice de la classe predite par le Cloud
%   field2 confiance en %
%   field3 temps d'execution de l'inference Cloud en ms
%   field4 accord avec la classe de la carte (1 = meme classe, 0 = different)
%
% Les champs 3 et 4 donnent la comparaison demandee au §3.c : temps
% d'execution et concordance des deux solutions SUR LA MEME OBSERVATION.

% ---------------------------------------------------------------- a remplir
canalMesures   = 0000000;             % Channel ID du canal de mesures
cleLecture     = "CLE_LECTURE";       % Read API Key du canal de mesures
canalResultats = 0000000;             % Channel ID du canal de resultats
cleEcriture    = "CLE_ECRITURE";      % Write API Key du canal de resultats
talkbackID     = "00000";             % identifiant du Talkback
cleTalkback    = "CLE_TALKBACK";      % API Key du Talkback

% Le fichier ONNX doit etre telechargeable : lien "raw" du depot GitHub.
urlModele = "https://raw.githubusercontent.com/EQUIPE/DEPOT/main/TP3_IA/modele_meteo.onnx";
% ---------------------------------------------------------------------------

classes = ["Clair", "Partiellement nuageux", "Couvert", "Precipitations"];

% Constantes de normalisation de l'entrainement (cf. modele_meteo.json).
% Identiques a celles du code embarque, a mettre a jour ensemble.
moyenne   = single([12.95862865, 74.98979187, 1016.40570068]);
ecartType = single([ 7.27653360, 17.85812759,    9.55475807]);

% Read data : derniere entree envoyee par la carte
donnees = thingSpeakRead(canalMesures, Fields=[1 2 3 4], NumPoints=1, ...
    ReadKey=cleLecture);

if isempty(donnees) || any(isnan(donnees(1:3)))
    error("Aucune mesure exploitable dans le canal %d.", canalMesures);
end

mesure     = single(donnees(1:3));
classeEdge = donnees(4);

% Load network (TensorFlow -> ONNX -> MatLab)
websave("modele_meteo.onnx", urlModele);
model = importONNXNetwork("modele_meteo.onnx", ...
    OutputLayerType="classification");

% Predict, en chronometrant la seule inference
x = (mesure - moyenne) ./ ecartType;
tic;
scores = predict(model, x);
dureeMs = toc * 1000;

% Get class. Les indices MATLAB commencent a 1, ceux de la carte a 0.
[prob, indice] = max(scores);
classeCloud = indice - 1;
accord = double(classeCloud == classeEdge);

fprintf("Cloud : %s (%.2f%%) en %.3f ms | carte : classe %d | accord : %d\n", ...
    classes(indice), prob*100, dureeMs, classeEdge, accord);

% §2.c : sauvegarde du resultat sur le canal specifique
thingSpeakWrite(canalResultats, [classeCloud, prob*100, dureeMs, accord], ...
    Fields=[1 2 3 4], WriteKey=cleEcriture);

% §2.d : depot du resultat dans la file Talkback. La carte execute
% "commands/execute" a chaque cycle et affiche la chaine recue.
webwrite("https://api.thingspeak.com/talkbacks/" + talkbackID + "/commands.json", ...
    "api_key", cleTalkback, "command_string", classes(indice));
