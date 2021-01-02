%%%This function creates "prepare.mat", including the following and more:
%%%sorted_form: sorted by func_id
%%%func_s_r: starting row number for each function, -1 for functions that don't show
function []=prepare(csv_path,mat_path)

%read form in ori_forma from .csv   
csv_name = csv_path;
ori_form = csvread(csv_name,1,0);

%sort the form by fun_id
row_num=size(ori_form,1);%number of all rows in the form
sorted_form=sortrows(ori_form,1);
max_func_num=sorted_form(row_num,1);

%every func starts with row func_s_r, not exists then -1
func_s_r=zeros(max_func_num+1,2); %funtion id starts with 0, so use +1 here to get
                                  %the number of distinct functions 

for i=1:max_func_num+1
    func_s_r(i,1)=i-1;
    func_s_r(i,2)=-1;
end

func_s_r( sorted_form(1,1)+1 , 2 )=1;

for i=2:row_num
    if sorted_form(i,1) ~= sorted_form(i-1,1)
        func_s_r( sorted_form(i,1)+1 , 2 )=i;
    end
end

clear i ori_form csv_name
save(mat_path);
end