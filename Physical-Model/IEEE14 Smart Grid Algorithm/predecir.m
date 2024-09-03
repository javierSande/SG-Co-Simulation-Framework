function [estado,linea]=predecir(dataset)


modelos = load('modelos.mat');

treeEstados = modelos.treeEstados;
treeLineas = modelos.treeLineas;

%disp('Corrientes:');
%dataset(68) = 99999;
%dataset(88) = 0;
%disp(dataset(67:86));
save('dataset.mat','dataset');

disp(dataset);
disp(size(dataset));

estado=treeEstados.predictFcn(dataset); 

if estado ~= 0
    linea=treeLineas.predictFcn(dataset);
else
    linea = 0;
end



