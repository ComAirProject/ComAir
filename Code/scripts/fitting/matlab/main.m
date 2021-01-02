%This is the entry function of my tool. Make sure that the mem_result.csv is in
%inputs_path and contains 4 columns of integer data. You can have one line of
%string type data at the top.

function []=main(inputs_path,bugname,LIMIT1,WINDOW)
%inputs_path is the full path of the directory that contains the bug
%bugname is the name of directory that is in inputs_path and contains mem_result.csv
%LIMIT1 is used to determine what functions are ignorable, should be 9 in
%our experiment setting
%WINDOW is used in funct(). We choose the worst
%point in every WINDOW points. Should be 1 in our experiment setting,
%meaning this functionality is never used


disp(['Functions whose record number is less than ',num2str(LIMIT1),' are ignored.']);

mat_path=[inputs_path,bugname,'/',bugname,'.mat'];
csv_path=[inputs_path,bugname,'/mem_result.csv'];
output_path=[inputs_path,bugname,'/complexity.csv'];
outmat_path=[inputs_path,bugname,'/all_variables.mat'];


if exist(mat_path,'file') == 0
    disp('Reading and processing data from .csv, saving to .mat');
    prepare(csv_path,mat_path);
end
%read from prepare.mat
load(mat_path);
   
%%%see part 1 in 'abandoned.m':focus on the biggest cost point of each 
%%%see part 2 in 'abandoned.m': row numbers for each run
%%%see part 5 in 'abandoned.m': input a function's ID to see its figures
whichn=zeros(max_func_num+1,3);
for i=1:(max_func_num+1)
    whichn(i,1)=i-1;
    whichn(i,2)=-2;
end

%continue_ornot=input('continue?[1/0]:');


for k=(0) : (max_func_num) %k is the function_ID
    
    %function whose ID=1 is in the second row of func_s_r
    disp([num2str(k),'/',num2str(max_func_num)]);
    if k>max_func_num
        disp('This is the last function. Please enter 0');
        break;
    end
    
    if func_s_r(k+1,2)==-1 
        whichn(k+1,2)=-3;
        continue;
    end
    %focus on one function    
    func_start_row=func_s_r(k+1,2);
    if func_start_row == -1
        %%%This may never happen. Maybe can delete this if
        error(['function ',num2str(k),' doen''t exist']);
        %%%
    end
    if k == max_func_num
        func_end_row=row_num;
    else
        next_exist_func=k+1;
        while func_s_r( next_exist_func+1 , 2 ) == -1
            next_exist_func=next_exist_func+1;
        end
        func_end_row= -1 + func_s_r( next_exist_func+1 , 2 );    
    end
    
     %functions whose records are less than LIMIT1 are ignored
    if (func_end_row-func_start_row)<LIMIT1
        whichn(k+1,2)=-2;
        continue;
    end
    
    func_form=sorted_form(func_start_row:func_end_row,[2 3]);
    
    [fun_form_new, whichn(k+1,2)] = funct(func_form,k,LIMIT1,WINDOW);
    clear cost_fun
    cost_fun(:,1)=fun_form_new(:,2);
    cost_fun=sortrows(cost_fun,1);
    if size(cost_fun,1) < 1 
        whichn(k+1,3) = 0;
    else
        whichn(k+1,3) = cost_fun(size(cost_fun,1),1);
    end
    close all
    clear func_form func_start_row func_end_row next_exist_func
end

for k=0:max_func_num
    j=max_func_num-k;
    if whichn(j+1,2)==-3 %-3: This function doesn't exist
        whichn(j+1,:)=[];
    end
end

% Let us filter some false positive of exponential functions
n_other_cost_max = -1;
n99_cost_max = -1;
for i = 1:size(whichn,1)
    if whichn(i,2) >= 990 && whichn(i,3) > n99_cost_max
        n99_cost_max = whichn(i,3);
    else
        if (whichn(i,2) <= 2 && whichn(i,2) >=0) && whichn(i,3) > n_other_cost_max
            n_other_cost_max = whichn(i,3);
        end
    end
end

if n_other_cost_max > 5*n99_cost_max
    for i = 1:size(whichn,1)
        if whichn(i,2) == 990
            whichn(i,2) = 0;
        end
        if whichn(i,2) == 991
            whichn(i,2) = 1;
        end
        if whichn(i,2) == 992
            whichn(i,2) = 2;
        end
    end
else
    for i = 1:size(whichn,1)
        if whichn(i,2) >= 990
                whichn(i,2) = 99;
        end
    end
end
%%%

count = 1;
i_cost=[-1,-1];
for i = 1:size(whichn,1)
    if whichn(i,2) == 2
        i_cost(count,1)=i;
        i_cost(count,2)=whichn(i,3);
        count = count + 1;
    end
end
i_cost=sortrows(i_cost,2);


if size(i_cost,1) >= 10
    for i = 1:round(size(i_cost,1)/5)
        if i_cost(i,2) < i_cost(size(i_cost,1),2)/2 
            whichn(i_cost(i),2) = 0;
        end
    end
end
%%%


csvwrite(output_path,whichn);
%what does whichn(:,2) means?
%-2: This function has too few points to judge its complexity
%draw figure

save(outmat_path);
end
    
    
        

