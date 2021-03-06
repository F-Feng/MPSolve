%% MPS_ROOTS function that approximate the roots of a polynomial specified
%% by the vector v using MPSolve and the algorithm specified by alg. 
%% alg can be 's' or 'u', depending if the algorithm MPS_ALGORITHM_SECULAR_GA or
%% the algorithm MPS_ALGORITHM_SECULAR_UNISOLVE are desired. 
%%
%% Author: Leonardo Robol <leonardo.robol@sns.it>
%% Copyright: 2011-2013 Leonardo Robol <leonardo.robol@sns.it>
%% License: GPLv3 or higher
function [x,r] = mps_roots(v, alg)
    
  if min(size(v)) ~= 1 || strcmp(class(v(1)), 'string')
    error('The input must be a 1D vector');
  end

  if nargin <= 1
     alg = 's';
  end

  if nargout > 1
    r = zeros(length(v)-1,1);

    if (~ (isfield (alg, 'radius') && alg.radius))
      warning('You need to specify the "radius" option to obtain the inclusion radius as output');
    end
  end

  if (isfield (alg, 'digits') && digits() < alg.digits)
     warning('Raising the number of digits of VPA to match the requested output digits');
     digits(alg.digits)
  end

  if isnumeric(v(1)) && ~(isfield(alg,'digits') && (alg.digits > 16))
    if (isfield (alg, 'radius') && alg.radius)
      [x,r] = mps_roots_double (v, alg);
    else
      x = mps_roots_double (v, alg);
    end
  else
    is_vpa = strcmp (class(v(1)), 'sym');
    
    % If the input is given in VPAs, convert them to string.
    vv = cell(1,length (v));
    if is_vpa
      for i = 1 : length (v)
        vv{i} = char(v(i));
      end
    else
      for i = 1 : length (v)
	vv{i} = num2str (v(i));
      end
    end
    v = vv;
    
    % FIXME: mps_roots_strings takes the coefficients in the wrong
    % order. 
    if (isfield (alg, 'radius') && alg.radius)
      [x,r] = mps_roots_string (vv(end:-1:1), alg);
    else
      x = mps_roots_string (vv(end:-1:1), alg);
    end

    % In case a cell output was returned, transform it in vpa
    if iscell (x)
       II = vpa(1i);
       y = vpa(zeros(1,size(x,1)));
       
       for i = 1 : size(x,1)
	 rp = strcat(x{i,1}, 'e', int2str (x{i,2}(1)));
	 ip = strcat(x{i,3}, 'e', int2str (x{i,4}(1)));

	 if rp(1) == '-'
	    rp = strcat('-0.', rp(2:end));
	 else
	     rp = strcat ('0.', rp);
	 end

	 if ip(1) == '-'
	    ip = strcat('-0.', ip(2:end));
	 else
	     ip = strcat ('0.', ip);
	 end

	 y(i) = vpa(rp) + II * vpa(ip);
       end

       x = y;
    end
  end
end
