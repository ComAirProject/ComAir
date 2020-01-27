function [func_form_new, whichn] = funct(func_form,funcid,LIMIT1,window)

%if two points are the same, only reserve one
func_form2=unique(func_form,'rows');
func_form3=sortrows(func_form2,2);
rms1=func_form3(:,1);
cost1=func_form3(:,2);

if size(rms1)<LIMIT1
    func_form_new=[0,0];
    whichn=-2;
    return;
end

%if several points' rms are the same, reserve the one with the highest cost
[~,uni_rownum2,~]=unique(rms1,'stable');
rms2=zeros(size(uni_rownum2,1),1);
cost2=zeros(size(uni_rownum2,1),1);

for i=1:size(uni_rownum2,1)-1 
    rms2(i)=rms1(uni_rownum2(i));
    if (uni_rownum2(i) + 1 == uni_rownum2(i+1))
        cost2(i)=cost1(uni_rownum2(i));
        continue;
    end
    start_row=uni_rownum2(i);%if rms=[...8,9,9,10...], then start_row will 
                              %  be the index of the first 9
    end_row=uni_rownum2(i+1)-1;
    cost2(i)=max(cost1(start_row:end_row));
end

start_row=size(rms1,1); % the last one in rms is dealt separately
if start_row<=1
    func_form_new=[0,0];
    whichn=-2;
    return;
end
while ( rms1(start_row) == rms1(start_row-1)) 
    start_row=start_row-1;
    if start_row<=1
        func_form_new=[0,0];
        whichn=-2;
        return;
    end
end
end_row=size(rms1,1);
rms2(end)=rms1(uni_rownum2(end));
cost2(end)=max(cost1(start_row:end_row));



%best, worst, average
if window>1 && size(rms2,1)>window %%% This is a magic number inputed in main
    window_num=(size(rms2,1)-mod(size(rms2,1),window))/window; % like 17/3=5
    for i=1:window_num
        rms3(i,1)=max(rms2(window*i-window+1:window*i));
        cost3_best(i,1)=min(cost2(window*i-window+1:window*i));
        cost3_worst(i,1)=max(cost2(window*i-window+1:window*i));
        cost3_average(i,1)=mean(cost2(window*i-window+1:window*i));
    end
else
    rms3=rms2;
    cost3_worst=cost2;
end

rms_out=rms3;
cost_out=cost3_worst;

%fit points as poly1, if the goodness(R^2) is greater than 0.995, then 
%translate all points so that the smallest one is at (0,0), and then delete
%the smallest one
p=polyfit(rms3,cost3_worst,1);
costfit=polyval(p,rms3);
costresid=cost3_worst-costfit;
SSresid=sum(costresid.^2);
SStotal=(length(cost3_worst)-1)*var(cost3_worst);
rsq=1-SSresid/SStotal;
if rsq > 0.999
    rms3=rms3-min(rms3);
    cost3_worst=cost3_worst-min(cost3_worst);
    rms3(1)=[];
    cost3_worst(1)=[];
end

%delete all cost(i) if cost(i)<0.01max(cost). Do this only when rest 
    %points are more than LIMIT1
for index_ignore=1:size(cost3_worst,1)
    if cost3_worst(index_ignore)>= 0.01*max(cost3_worst)
        break;
    end
end

if (size(cost3_worst,1)-index_ignore)>=LIMIT1
    for i=1:index_ignore
        rms3(1)=[];
        cost3_worst(1)=[];
    end
end

%The following is to ensure that robust fitting will have enough points

%if min(rms3)<1, log(rms3) could be negative
if min(rms3)<=1 
    if min(rms3)~=0
        rms3=2*rms3/min(rms3);
    else
        size_rms3=size(rms3,1);
        for i=1:size_rms3
            j=size_rms3+1-i;
            if rms3(j)==0
                rms3(j)=[];
                cost3_worst(j)=[];
            end
        end
        if min(rms3)<=1
            rms3=2*rms3/min(rms3);
        end
    end
end

if size(rms3,1)<LIMIT1
    func_form_new=[1,1;1,1];
    whichn=-2;
    return;
end

%guess complexity is nlogn
cost4=cost3_worst./(rms3.*log(rms3));
cost4=cost4/mean(cost4); %normalization by mean
robustfitre=robustfit(rms3,cost4); %robustfit
slope(4)=robustfitre(2); % slope of robustfit line of N guess

%guess complexity is n^2
cost5=cost3_worst./(rms3.^2);
cost5=cost5/mean(cost5); 
robustfitre=robustfit(rms3,cost5); %robustfit
slope(5)=robustfitre(2); % slope of robustfit line of N guess

%guess complexity is sqrt(n), just to determine whether a function is O(1)
%or O(N)
cost6=cost3_worst./sqrt(rms3);
cost6=cost6/mean(cost6); 
robustfitre=robustfit(rms3,cost6); %robustfit
slope(6)=robustfitre(2); % slope of robustfit line of N guess

%represent the complexity using whichn
%complexity:|costant_or_unknown   less_than_NlogN more_than_NlogN_not_exp(N) exp(N)|
%   whichn :|   0                       1                2                     99  |

whichn=1;
if slope(4)>0
    if abs(slope(4))>abs(slope(5))
        whichn=2;
    end
end

if whichn==1
    if  slope(6)<0 && abs(slope(4))>abs(slope(6))
        whichn=0;
    end
end

%see if the fit curve is always increasing
if whichn==2
    poly_1=polyfit(rms3,cost3_worst,1);
    
    if poly_1(1) < 0 
        whichn = 0;
    end

end



%if the function fits N^2 very well, then don't consider exp(n)
poly_2=polyfit(rms1,cost1,2);
yfit2=polyval(poly_2,rms1);
yresid2=cost1-yfit2;
SSresid2=sum(yresid2.^2);
SStotal2=(length(cost1)-1)*var(cost1);
rsq2=1-SSresid2/SStotal2;
if rsq2 < 0.4 
    
    % guess complexity is exp(n)
    cost7=log(cost3_worst);%cost7 ~ rms
    for i=1:size(cost7,1)
        if cost7(i,1)==0
            func_form_new=zeros(size(rms_out,1),2);
            func_form_new(:,1)=rms_out(:);
            func_form_new(:,2)=cost_out(:);
            return
        end
    end

    cost7=cost7./(log(rms3).^2);
    cost7=cost7/mean(cost7); %normalization by mean
    

    robustfitre=robustfit(rms3,cost7); %robustfit
    slope(7)=robustfitre(2); % slope of robustfit line of N guess
    if slope(7)>0 
            if whichn == 1
                whichn = 991;
            end
            if whichn == 2
                whichn = 992;
            end
            if whichn == 0
                whichn = 990;
            end
    %         disp(['>>>>>>>>O(expN):',num2str(funcid)]);
            diff=log(max(rms3))/log(min(rms3));
            if (diff)<1.3
    %             disp(['>>>>>>>>log(max(rms))/log(min(rms)) = ',num2str(diff),' which may be too small.']);
            end                
    end
end

func_form_new=zeros(size(rms_out,1),2);
func_form_new(:,1)=rms_out(:);
func_form_new(:,2)=cost_out(:);

end
