% TP3 ETRS923 - §2.a et §2.b : inference du reseau MeteoStat sous MATLAB.
%
% Meme structure que l'exemple MNIST du sujet : chargement du reseau depuis
% l'ONNX, preparation de l'entree, prediction, lecture de la classe.
%
% Entree  : temperature (degC), humidite relative (%RH), pression (hPa)
% Sortie  : distribution de probabilite sur les 4 classes
%
% Prerequis : Deep Learning Toolbox + "Converter for ONNX Model Format".
% Le fichier modele_meteo.onnx est produit par ../3_export_onnx.py.

% Load network (§2.a : TensorFlow -> ONNX -> MatLab)
% Si MATLAB ne reconnait pas le format de l'entree, ajouter :
%     InputDataFormats="BC"
model = importONNXNetwork("modele_meteo.onnx", ...
    OutputLayerType="classification");

% Classes, dans l'ordre des sorties du reseau (cf. ../modele_meteo.json)
classes = ["Clair", "Partiellement nuageux", "Couvert", "Precipitations"];

% Normalisation centree-reduite : constantes de l'entrainement, a mettre a
% jour apres tout reentrainement (cf. ../modele_meteo.json). Elles doivent
% etre identiques a celles du code embarque (Core/Src/modele_ia.c), sinon
% le Cloud et la carte ne classent pas la meme chose.
moyenne   = single([12.95862865, 74.98979187, 1016.40570068]);
ecartType = single([ 7.27653360, 17.85812759,    9.55475807]);

% Read measurement : une observation relevee sur la carte
mesure = single([24.30, 41.10, 996.84]);   % degC, %RH, hPa

% Normalize
x = (mesure - moyenne) ./ ecartType;

% Predict
scores = predict(model, x);

% Get class
[prob, indice] = max(scores);

% Print
fprintf("Predicted class: %s (confidence %.2f%%)\n", classes(indice), prob*100);

% Reference : pour cette mesure, onnxruntime donne "Partiellement nuageux"
% a 39,56 %, et la carte a affiche la meme classe a 39,55 %. MATLAB doit
% donc retrouver cette valeur, sinon la normalisation est a verifier.
