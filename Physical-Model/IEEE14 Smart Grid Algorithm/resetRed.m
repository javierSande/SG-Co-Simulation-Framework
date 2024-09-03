% reset script

serie = [27824000,25076000,11817820,6916420,15090180,8831580,...
    20484800,11988800,19805590,11591290,16845710,9859010,12028310, ...
    7039610,17472840,10226040,14912240,8727440,14706090,8606790,...
    18199790,10651490,13888000,8128000];

direccion = "Ieee14_CC/";
% 3. Crear vectores strings

sourcesName = "Three-Phase Source";
sourcesNameInd = [1 2 3 6 8];

loadsName = "Three-Phase Series RLC Load";
loadsNameInd = [2 3 4 5 6 9 10 11 12 13 14];


% sources
aux = direccion + sourcesName + sourcesNameInd(2);

set_param(aux, 'Pref', mat2str(serie(1)));

aux = direccion + sourcesName + sourcesNameInd(3);

set_param(aux, 'Pref', mat2str(serie(2)));

% loads
j = 3;
for i = 1:length(loadsNameInd)
    aux = direccion + loadsName + loadsNameInd(i);
    set_param(aux, 'ActivePower', mat2str(serie(j)));
    set_param(aux, 'InductivePower', mat2str(serie(j+1)));
    j = j + 2;
end

B1_L5 = "B1_L5";
B1_L4 = "B1_L4";
B2_G1 = "B2_G1";
B2_L5 = "B2_L5";
B2_L4 = "B2_L4";
B2_G3 = "B2_G3";
B3_L4 = "B3_L4";
B6_L11 = "B6_L11";
B6_L12 = "B6_L12";
B6_L13 = "B6_L13";
B6_L13_2 = "B6_L13_2";
B6_L10 = "B6_L10";
B6_L14 = "B6_L14";
B8_L5 = "B8_L5";
B8_L14 = "B8_L14";
B8_L10 = "B8_L10";

% 4. Cond. Iniciales

% 4.1. Inicializar los CC

aux = direccion + "CC_" + "L0";
set_param(aux, 'Value', mat2str(0));

aux = direccion + "CC_" + "L1";
set_param(aux, 'Value', mat2str(0));

aux = direccion + "CC_" + "L2";
set_param(aux, 'Value', mat2str(0));

aux = direccion + "CC_" + "L3" ;
set_param(aux, 'Value', mat2str(0));

aux = direccion + "CC_" + "L4" ;
set_param(aux, 'Value', mat2str(0));

aux = direccion + "CC_" + "L5" ;
set_param(aux, 'Value', mat2str(0));

aux = direccion + "CC_" + "L6" ;
set_param(aux, 'Value', mat2str(0));

aux = direccion + "CC_" + "L7" ;
set_param(aux, 'Value', mat2str(0));

aux = direccion + "CC_" + "L8" ;
set_param(aux, 'Value', mat2str(0));

aux = direccion + "CC_" + "L9";
set_param(aux, 'Value', mat2str(0));

aux = direccion + "CC_" + "L10";
set_param(aux, 'Value', mat2str(0));

aux = direccion + "CC_" + "L11" ;
set_param(aux, 'Value', mat2str(0));

aux = direccion + "CC_" + "L12" ;
set_param(aux, 'Value', mat2str(0));

aux = direccion + "CC_" + "L13" ;
set_param(aux, 'Value', mat2str(0));

aux = direccion + "CC_" + "L14" ;
set_param(aux, 'Value', mat2str(0));

aux = direccion + "CC_" + "L15";
set_param(aux, 'Value', mat2str(0));