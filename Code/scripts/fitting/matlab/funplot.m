% A function to see the figure. You don't need this function to generate
% complexity.csv
clear funid_form
close all

funid=input('input the function ID:');
while funid>=0
    
    func_start_row=func_s_r(funid+1,2);
    if func_start_row == -1
       error(['function ',num2str(funid),' doen''t exist']);
    end
    if funid == max_func_num
        func_end_row=row_num;
    else
        next_exist_func=funid+1;
        while func_s_r( next_exist_func+1 , 2 ) == -1
            next_exist_func=next_exist_func+1;
        end
            func_end_row= -1 + func_s_r( next_exist_func+1 , 2 );    
    end
    funid_form=sorted_form(func_start_row:func_end_row,2:3);
    funid_form=sortrows(funid_form,1);
    x=funid_form(:,1);
    y=funid_form(:,2);
    figure(1)
    scatter(x,y);

    p1=polyfit(x,y,1);
    yfit1=polyval(p1,x);
    yresid1=y-yfit1;
    SSresid1=sum(yresid1.^2);
    SStotal1=(length(y)-1)*var(y);
    f1=fit(x,y,'poly1')
    rsq1=1-SSresid1/SStotal1
    hold on 
    plot(x,yfit1)

    p2=polyfit(x,y,2);
    symmetric_axis = - p2(2) / 2 / p2(1);
    yfit2=polyval(p2,x);
    yresid2=y-yfit2;
    SSresid2=sum(yresid2.^2);
    SStotal2=(length(y)-1)*var(y);
    f2=fit(x,y,'poly2') %use fit function instead of polyfit
    rsq2=1-SSresid2/SStotal2
    plot(x,yfit2)
 

    [new_form,whichn_this]=funct(funid_form,funid,LIMIT1,WINDOW);
    whichn_this
    funid=input('input the function ID:');
    clear funid_form
    close all
end