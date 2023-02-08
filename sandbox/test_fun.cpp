////////////////////////////
// Test functions to be included in HighFreq.cpp
////////////////////////////

// Compile this file in R by running this command:
// Rcpp::sourceCpp(file="/Users/jerzy/Develop/HighFreq/sandbox/test_fun.cpp")

// [[Rcpp::depends(RcppArmadillo)]]
#include <RcppArmadillo.h>
#include <vector>
using namespace arma;
// Use STL
using namespace std;


// Calculate the alpha and beta regression coefficients
// without using the Moore-Penrose inverse arma::pinv().
// [[Rcpp::export]]
arma::mat calc_betas(const arma::mat& response, 
                     const arma::mat& predictor) {
  
  // arma::uword nrows = predictor.n_rows;
  // arma::uword ncols = predictor.n_cols;
  // Response and Predictor means
  arma::mat respmeans = arma::mean(response, 0);
  arma::mat predmeans = arma::mean(predictor, 0);
  arma::mat predz = predictor.each_row() - predmeans;
  
  // Initialize the variables
  arma::mat covarespred = arma::trans(response.each_row()-respmeans)*predz;
  arma::mat covarpred = arma::trans(predz)*predz;
  arma::mat betas = covarespred*arma::inv(covarpred);
  arma::mat alphas = respmeans - arma::dot(betas, predmeans);
  
  return arma::join_rows(alphas, betas);
  
}  // end calc_betas


// Calculate a limited number of eigen values.
// arma::eigs_sym() works only for sparse matrices which are 
// not standard R matrices.
// [[Rcpp::export]]
Rcpp::List calc_eigensp(const arma::sp_mat& matrixv, const arma::uword& neigen) {
  
  arma::mat eigenvec;
  arma::vec eigenval;
  arma::eigs_sym(eigenval, eigenvec, matrixv, neigen);

  // Reverse the order of elements from largest eigenvalue to smallest, similar to R
  return Rcpp::List::create(Rcpp::Named("values") = arma::flipud(eigenval),
                            Rcpp::Named("vectors") = arma::fliplr(eigenvec));
  
}  // end calc_eigensp


// [[Rcpp::export]]
void push_covar(const arma::rowvec& newdata, // Row of new asset returns
                arma::mat& covmat,  // Covariance matrix
                arma::rowvec& meanv, // Trailing means of the returns
                const double& lambdacov) { // Covariance decay factor
  
  double lambda1 = 1-lambdacov;
  
  // Update the means of the returns
  meanv = lambdacov*meanv + lambda1*newdata;
  // Calculate the de-meaned returns
  arma::rowvec datav = (newdata - meanv);
  
  // Update the covariance of the returns
  covmat = lambdacov*covmat + lambda1*arma::trans(datav)*datav;
  
}  // end push_covar


// Slower version below - because push_covar() calls arma::trans()
// [[Rcpp::export]]
arma::mat run_covar(const arma::mat& tseries, double lambdacov) {
  
  arma::uword nrows = tseries.n_rows;
  arma::uword ncols = tseries.n_cols;
  arma::mat vars(nrows, ncols);
  arma::mat covar(nrows, 1);
  arma::rowvec meanv = tseries.row(0);
  arma::mat covmat = arma::trans(meanv)*meanv;
  
  // Copy the covariance data
  vars.row(0) = arma::trans(covmat.diag());
  covar.row(0) = covmat(0, 1);
  
  // Perform loop over the rows
  for (arma::uword it = 1; it < nrows; it++) {
    // Update the covariance matrix
    push_covar(tseries.row(it), covmat, meanv, lambdacov);
    // Copy the covariance data
    vars.row(it) = arma::trans(covmat.diag());
    covar.row(it) = covmat(0, 1);
  }  // end for
  
  return arma::join_rows(covar, vars);
  
  // Faster code below - because it doesn't call arma::trans()
  // It doesn't call push_covar()
  // arma::uword nrows = tseries.n_rows;
  // arma::uword ncols = tseries.n_cols;
  // arma::mat meanm = tseries.row(0);
  // arma::mat meand(1, ncols);
  // arma::mat vars(nrows, ncols);
  // arma::mat covar(nrows, 1);
  // double lambda1 = 1-lambda;
  // 
  // // Perform loop over the rows
  // vars.row(0) = arma::square(tseries.row(0));
  // covar.row(0) = tseries(0, 0)*tseries(0, 1);
  // for (arma::uword it = 1; it < nrows; it++) {
  //   // Calculate the mean as the weighted sum
  //   meanm = lambda*meanm + lambda1*tseries.row(it);
  //   meand = tseries.row(it) - meanm;
  //   // Calculate the covariance as the weighted sum of products of returns
  //   vars.row(it) = lambda*vars.row(it-1) + lambda1*arma::square(meand);
  //   covar.row(it) = lambda*covar.row(it-1) + lambda1*(meand(0)*meand(1));
  // }  // end for
  // 
  // return arma::join_rows(covar, vars);
  // 
  
}  // end run_covar


// [[Rcpp::export]]
void push_eigen(const arma::rowvec& newdata, // Row of new asset returns
                arma::mat& covmat,  // Covariance matrix
                arma::vec& eigenval, // Eigen values
                arma::mat& eigenvec, // Eigen vectors
                arma::rowvec& eigenret, // Row of eigen portfolio returns
                arma::rowvec& meanv, // Trailing means of the returns
                const double& lambdacov) { // Covariance decay factor
  
  // Scale the returns by the volatility
  arma::rowvec varv = arma::trans(covmat.diag());
  varv.replace(0, 1);
  arma::rowvec newdatas = newdata/arma::sqrt(varv);
  // Calculate the eigen portfolio returns - the products of the previous eigen vectors times the scaled returns
  eigenret = newdatas*eigenvec;
  // Update the covariance matrix
  push_covar(newdatas, covmat, meanv, lambdacov);
  // Calculate the eigen decomposition
  arma::eig_sym(eigenval, eigenvec, covmat);
  
}  // end push_eigen


// [[Rcpp::export]]
arma::mat run_pca(const arma::mat& tseries, 
                  const double& lambdacov) { // Covariance decay factor
  
  arma::uword nrows = tseries.n_rows;
  arma::uword ncols = tseries.n_cols;
  arma::vec eigenval(ncols);
  arma::mat eigenvec(ncols, ncols);
  arma::rowvec eigenret(ncols);
  arma::mat eigenretmat(nrows, ncols);
  
  // Initialize with first row of data
  arma::rowvec meanv = tseries.row(0);
  arma::mat covmat = arma::trans(meanv)*meanv;

  // Perform loop over the rows
  for (arma::uword it = 0; it < nrows; it++) {
    // Update the covariance matrix
    push_eigen(tseries.row(it), covmat, eigenval, eigenvec, eigenret, meanv, lambdacov);
    eigenretmat.row(it) = eigenret;
  }  // end for

  return eigenretmat;
  
}  // end run_pca


// [[Rcpp::export]]
arma::mat run_portf(const arma::mat& tseries, 
                       const arma::uword& dimax, // Dimension reduction
                       const double& lambda, // Returns decay factor
                       const double& lambdacov, // Covariance decay factor
                       bool scalit = true, // Scale the returns by the volatility?
                       bool flipc = true) { // Flip the signs of the principal component weights (eigenvectors)?
  
  arma::uword nrows = tseries.n_rows;
  arma::uword ncols = tseries.n_cols;
  arma::vec eigenval(ncols); // Eigen values
  arma::mat eigenvec(ncols, ncols); // Eigen vectors
  arma::mat eigenvecp(nrows, 2*ncols+2); // Past eigen vectors
  arma::mat eigenout(nrows, 2*ncols+2); // First two eigen vectors
  arma::rowvec eigenret(ncols, fill::zeros); // Returns of principal components
  arma::rowvec eigenretm(ncols, fill::zeros); // Average of principal components returns
  arma::rowvec eigenvar(ncols, fill::zeros); // Variance of principal component returns
  arma::rowvec weightv(ncols, fill::ones); // Principal component weights
  arma::rowvec weightp(ncols, fill::ones); // Past principal component weights
  arma::rowvec weightpp(ncols, fill::ones); // Past past principal component weights
  arma::colvec portfret(nrows); // Portfolio strategy returns
  double weightd; // Sum of squared principal component weights
  double lambda1 = 1-lambda;
  
  // Initialize the covariance matrix with first row of data
  arma::rowvec meanv = tseries.row(0);
  arma::mat covmat = arma::trans(meanv)*meanv;
  // Initialize the eigen decomposition
  arma::eig_sym(eigenval, eigenvecp, covmat);
  
  // Perform loop over the rows
  for (arma::uword it = 0; it < nrows; it++) {
    // Scale the returns by the volatility
    arma::rowvec varv = arma::trans(covmat.diag());
    varv.replace(0, 1);
    arma::rowvec newdatas = tseries.row(it)/arma::sqrt(varv);
    // Calculate the eigen portfolio returns - the products of the previous eigen vectors times the scaled returns
    eigenret = newdatas*eigenvec;
    // Calculate the portfolio returns - the products of the previous weights times the eigen portfolio returns
    portfret(it) = arma::dot(eigenret, weightpp);
    // Update the covariance matrix
    if (scalit) {
      push_covar(newdatas, covmat, meanv, lambdacov);
    } else {
      push_covar(tseries.row(it), covmat, meanv, lambdacov);
    }  // end if
    // Calculate the eigen decomposition
    arma::eig_sym(eigenval, eigenvec, covmat);
    // Calculate the trailing mean and variance of eigen returns
    eigenretm = lambda*eigenretm + lambda1*eigenret;
    eigenvar = lambda*eigenvar + lambda1*arma::square(eigenret - eigenretm);
    eigenvar.replace(0, 1);
    // Calculate the principal component weights equal to the Kelly ratios
    weightv = eigenretm/eigenvar;
    // Apply dimension reduction to the weights
    weightv.subvec(dimax, ncols-1).fill(0);
    // Scale the weights so their sum of squares is equal to one
    weightd = std::sqrt(arma::sum(arma::square(weightv)));
    if (weightd == 0) weightd = 1;
    weightv = weightv/weightd;
    // Remember the past weights
    weightpp = weightp;
    weightp = weightv;
    // Flip the eigen vector signs if the eigen vectors change too much
    if (flipc) {
      for (arma::uword it=0; it < ncols; it++) {
        if (arma::sum(arma::square(eigenvec.col(it) - eigenvecp.col(it))) > 2.0)
          eigenvec.col(it) = -eigenvec.col(it);
      }  // end for
      eigenvecp = eigenvec;
    } // end if 
    // Copy the first two eigen vectors
    eigenout.row(it)(0) = eigenval(0);
    eigenout.row(it).subvec(1, ncols) = arma::trans(eigenvec.col(0));
    eigenout.row(it)(ncols) = eigenval(1);
    eigenout.row(it).subvec(ncols+1, 2*ncols) = arma::trans(eigenvec.col(1));
  }  // end for
  
  // Return the portfolio returns and the first two eigen vectors
  return arma::join_rows(portfret, eigenout);
  
}  // end run_portf


// [[Rcpp::export]]
void run_sgao(arma::colvec& eigenval, // eigen values
             arma::mat& eigenvec, // eigen vectors
             const arma::colvec& newdata, // row of new data
             arma::colvec& meanv, // mean of the data
             arma::colvec& volv, // volatility of the data
             const double& lambda, // decay factor to multiply the past mean and volatility
             const double& gamma) { // gain factor to multiply the past eigenelements
  
  // Update the mean and volatility of the data
  meanv = lambda*meanv + (1-lambda)*newdata;
  // volv = lambda*volv + (1-lambda)*arma::square(newdata-meanv);
  // arma::colvec datav = (newdata-meanv)/volv;
  arma::colvec datav = (newdata-meanv);
  // std::cout << "datav: " << std::endl << arma::trans(datav) << std::endl;
  
  // Calculate the phis - the products of the eigen vectors times the new data
  arma::mat vecd = arma::trans(eigenvec)*datav;
  // std::cout << "vecd: " << std::endl << arma::trans(vecd) << std::endl;
  // Calculate the updated eigen values
  arma::colvec neigenval = (1-gamma)*eigenval + gamma*arma::square(vecd);
  // std::cout << "eigenval: " << std::endl << eigenval << std::endl;
  // std::cout << "neigenval: " << std::endl << arma::trans(neigenval) << std::endl;
  
  // Calculate diagonal matrix of gain factors
  arma::uword ncols = eigenvec.n_cols;
  arma::mat dgamma = arma::mat(ncols, ncols, arma::fill::zeros);
  dgamma.diag().fill(gamma);
  
  // Perform QR decomposition of Q to get the eigen vectors W
  arma::mat W, R, Q = eigenvec;
  Q += datav*arma::trans(vecd)*dgamma;
  // std::cout << "Q: " << std::endl << Q << std::endl;
  arma::qr_econ(W, R, Q);
  
  arma::uvec sorti = arma::sort_index(neigenval, "descend");
  // std::cout << "sorti: " << std::endl << sorti << std::endl;
  neigenval = neigenval(sorti);
  eigenval = neigenval;
  W = W.cols(sorti);
  eigenvec = W;
  
}  // end run_sgao





// [[Rcpp::export]]
void sgapca_exCn(arma::colvec& eigenval, 
                 arma::mat& Qs, 
                 const arma::colvec& x,
                 arma::colvec& meanv,
                 const double& n,
                 const double& gamma) {
  
  arma::mat W, R, Q = Qs;
  double lambda = 1/(n+1);
  meanv = (1-lambda)*meanv + lambda*x;
  arma::colvec x1 = x - meanv;
  
  arma::mat ux = arma::trans(Qs)*x1;
  // std::cout << "ux: " << std::endl << ux << std::endl;
  // arma::colvec gamy = gamma*ux;
  arma::uword ncols = Qs.n_cols;
  arma::mat dgamma = arma::mat(ncols, ncols, arma::fill::zeros);
  dgamma.diag().fill(gamma);
  // arma::mat dgamma = arma::diagmat(gamma);
  Q += x1*arma::trans(ux)*dgamma;
  // std::cout << "Q: " << std::endl << Q << std::endl;
  arma::qr_econ(W, R, Q);
  arma::colvec neigenval = (1 - gamma)*eigenval + gamma*arma::square(ux);
  // std::cout << "eigenval: " << std::endl << eigenval << std::endl;
  // std::cout << "neigenval: " << std::endl << neigenval << std::endl;
  // return Rcpp::wrap(W);
  arma::uvec sorti = arma::sort_index(neigenval, "descend");
  
  neigenval = neigenval(sorti);
  eigenval = neigenval;
  W = W.cols(sorti);
  Qs = W;
  
  // return Rcpp::List::create(Rcpp::Named("values") = neigenval,
  //                           Rcpp::Named("vectors") = W);
  // Rcpp::Named("ux") = arma::sort(ux, "descend"));
  
}  // end sgapca_exCn




// [[Rcpp::export]]
Rcpp::NumericMatrix sgapca_nnC (const arma::mat& Q, const arma::colvec& x, 
                                const arma::colvec& y, const arma::colvec& gamma) {
  
  int m = Q.n_rows, n = Q.n_cols;
  arma::colvec gamy = gamma % y;
  arma::colvec b = y(0)*Q.col(0);
  arma::mat A(m,n);
  A.col(0) = Q.col(0) - gamy(0)*b;	
  for (int i=1; i<n; i++) {
    b += y(i-1)*Q.col(i-1) + y(i)*Q.col(i);
    A.col(i) = Q.col(i) - gamy(i)*b;
  }		
  A += x*arma::trans(gamy);
  return Rcpp::wrap(A);
  
}  // end calc_eigensp



// [[Rcpp::export]]
Rcpp::NumericMatrix ghapca_C (const arma::mat& Q, const arma::colvec& x, 
                              const arma::colvec& y, const arma::colvec& gamma) {
  
  int m = Q.n_rows, n = Q.n_cols;
  arma::colvec gamy = gamma % y;
  arma::colvec b = y(0)*Q.col(0);
  arma::mat A(m,n);
  A.col(0) = Q.col(0) - gamy(0)*b;	
  for (int i=1; i<n; i++) {
    b += y(i)*Q.col(i);
    A.col(i) = Q.col(i) - gamy(i)*b;
  }		
  A += x*arma::trans(gamy);
  return Rcpp::wrap(A);
  
}  // end calc_eigensp


// [[Rcpp::export]]
Rcpp::List ccipca_C (arma::colvec lambda, arma::mat U, 
                     arma::colvec x, int n, int q, double l, double tol) {
  
  int i, d = x.n_elem, k = lambda.n_elem;
  if (q != k) {
    U.resize(d,q);
    lambda.resize(q);
  }
  arma::colvec v(d);  
  double f = (1.0+l)/(1.0+n);
  double nrm;
  
  for (i=0; i<q; i++) {  
    
    nrm = arma::norm(x);
    if (nrm < tol) {
      lambda.tail(q-i) = (1-f)*lambda.tail(q-i);
      break;
    }			
    
    if (i == n) {
      lambda[i] = nrm;			
      U.col(i) = x/nrm;
      break;
    }
    
    v = ((1-f)*arma::as_scalar(lambda[i]))*U.col(i) + 
      (f*arma::dot(U.col(i),x))*x;
    nrm = arma::norm(v);
    if (nrm < tol) {
      lambda[i] = 0.0;
      break; 
    } 	
    lambda[i] = nrm; 
    U.col(i) = v/nrm;
    x -= arma::dot(U.col(i), x)*U.col(i);		
    
  }
  
  // 	typedef	std::vector<double> stdvec;
  // 	stdvec lambdav = arma::conv_to< stdvec >::from(lambda); 
  // 	return Rcpp::List::create(Rcpp::Named("values") = lambdav, 
  // 			Rcpp::Named("vectors") = U);
  return Rcpp::List::create(Rcpp::Named("values") = lambda, 
                            Rcpp::Named("vectors") = U);
  
}  // end calc_eigensp


