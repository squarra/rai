/*  ------------------------------------------------------------------
    Copyright (c) 2011-2024 Marc Toussaint
    email: toussaint@tu-berlin.de

    This code is distributed under the MIT License.
    Please see <root-path>/LICENSE for details.
    --------------------------------------------------------------  */

#include "spline.h"
#include "../Core/util.h"

#include <math.h>

//==============================================================================
//
// Spline
//

namespace rai {

void BSpline::setKnots(uint _degree, const arr& times){
  degree = _degree;
  uint nCtrls = times.N + 2*(degree/2);
  uint nKnots = nCtrls+degree+1;
  knots.resize(nKnots);

  for(uint i=0; i<nKnots; i++) {
    if(i<=degree) knots(i)=times.elem(0);
    else if(i>=nCtrls) knots(i)=times.elem(-1);
    else if((degree%2)) {
      knots(i) = times(i-degree);
    } else {
      knots(i) = .5*(times(i-degree-1)+times(i-degree));
    }
  }
}

void BSpline::calcB(double t, uint derivatives, bool calc_JBtimes) {
  B.resize(knots.N-degree, degree+1).setZero();
  if(derivatives>0) Bdot.resize(knots.N-degree, degree+1).setZero();
  if(derivatives>1) Bddot.resize(knots.N-degree, degree+1).setZero();
  if(calc_JBtimes) JBtimes.resize(B.d0, B.d1, knots.N).setZero();
//  CHECK_LE(knots(0), t, "");
//  CHECK_LE(t, knots(-1), "");

//profiling: defines to make access faster - avoiding checks
#define _knots(i) knots.p[i]
#define _B(i,j) B.p[(i)*B.d1+j]
#define _DIV(x, y) (x==0.?0.:(y==0.?0.:x/y))

  //initialize rank 0
  for(uint k=0; k<B.d0; k++) {
    if(_knots(k)<=t && k+1<knots.N && t<_knots(k+1)) _B(k, 0)=1.;
  }

  //recursion
  for(uint p=1; p<=degree; p++) {
    for(uint i=0; i<B.d0; i++) {
      if(i+p<knots.N) {
        double xnom = t - _knots(i);
        double xden = _knots(i+p) - _knots(i);
        if(xden!=0.) {
          double x = _DIV(xnom, xden);
          _B(i, p) = x * _B(i, p-1);
          if(derivatives>0) Bdot(i, p) = _DIV(1., xden) * B(i,p-1) + x * Bdot(i,p-1);
          if(derivatives>1) Bddot(i, p) = _DIV(2., xden) * Bdot(i,p-1) + x * Bddot(i,p-1);
          if(calc_JBtimes) {
            JBtimes(i, p, i) += (-1./xnom + 1./xden) * x * _B(i, p-1);
            JBtimes(i, p, i+p) += (-1./xden) * x * _B(i, p-1);
//            J(i,p,{}) += x * J(i, p-1, {});
            for(double* a=&JBtimes(i, p, 0), *b=&JBtimes(i, p-1, 0), *stop=a+JBtimes.d2; a!=stop; a++, b++) *a += x * (*b);
          }
        }
      }
      if(i+1<knots.N && i+p+1<knots.N && i+1<B.d0) {
        double ynom = _knots(i+p+1) - t;
        double yden = _knots(i+p+1) - _knots(i+1);
        if(yden!=0.) {
          double y = _DIV(ynom, yden);
          _B(i, p) += y * _B(i+1, p-1);
          if(derivatives>0) Bdot(i, p) += _DIV(-1., yden) * B(i+1,p-1) + y * Bdot(i+1,p-1);
          if(derivatives>1) Bddot(i, p) += _DIV(-2., yden) * Bdot(i+1,p-1) + y * Bddot(i+1,p-1);
          if(calc_JBtimes) {
            JBtimes(i, p, i+1) += (1./yden) * y * _B(i+1, p-1);
            JBtimes(i, p, i+p+1) += (1./ynom - 1./yden) * y * _B(i+1, p-1);
//            J(i,p,{}) += y * J(i+1, p-1, {});
            for(double* a=&JBtimes(i, p, 0), *b=&JBtimes(i+1, p-1, 0), *stop=a+JBtimes.d2; a!=stop; a++, b++) *a += y * (*b);
          }
        }
      }
    }
  }

  //special case: outside the knots
  if(t>=knots(-1)) B(-2,-1) = 1.; //for(uint p=0;p<=degree;p++) B(B.d0-degree-1+p, p) = 1.;
  // if(t<knots(0))   for(uint p=0;p<=degree;p++) B(0, p) = 1.;

#undef _knots
#undef _B
#undef _DIV
}

arr BSpline::getBmatrix(const arr& sampleTimes, bool startDuplicates, bool endDuplicates){
  CHECK(knots.N, "need to set knots first");
  uint m = knots.N-1;
  uint K = m - degree - 1;
  arr Bmatrix(sampleTimes.N, K+1);
  for(uint t=0; t<sampleTimes.N; t++){
    calcB(sampleTimes(t));
    for(uint i=0;i<=K;i++) Bmatrix(t,i) = B(i, degree);
  }
  if(startDuplicates||endDuplicates){
    Bmatrix = ~Bmatrix;
    if(startDuplicates){
      Bmatrix[1] += Bmatrix[0];
      Bmatrix.delRows(0);
    }
    if(endDuplicates){
      Bmatrix[-2] += Bmatrix[-1];
      Bmatrix.delRows(-1);
    }
    Bmatrix = ~Bmatrix;
  }
  return Bmatrix;
}

//==============================================================================

void BSpline::clear() {
  ctrlPoints.clear();
  knots.clear();
}

void BSpline::setCtrlPoints(const arr& points, bool addStartDuplicates, bool addEndDuplicates, const arr& setStartVel, const arr& setEndVel) {
  CHECK(knots.N, "need to set knots first");
  if(!points.d1){ ctrlPoints.resize(points.d0+2*(degree/2), points.d1); return; }

  ctrlPoints = points;
  for(uint i=0; i<degree/2; i++) {
    if(addStartDuplicates) ctrlPoints.prepend(points[0]);
    if(addEndDuplicates) ctrlPoints.append(points[-1]);
  }
  CHECK_EQ(ctrlPoints.d0, knots.N-degree-1, "");

  if(!!setStartVel && setStartVel.N) setDoubleKnotVel(-1, setStartVel);
  if(!!setEndVel && setEndVel.N) setDoubleKnotVel(points.d0-1, setEndVel);
}

BSpline& BSpline::set(uint _degree, const arr& points, const arr& times, const arr& startVel, const arr& endVel) {
  CHECK_EQ(times.nd, 1, "");
  CHECK_EQ(points.nd, 2, "");
  CHECK_EQ(points.d0, times.N, "");

  setKnots(_degree, times);
  setCtrlPoints(points, true, true, startVel, endVel);

  return *this;
}

void BSpline::overwriteSmooth(const arr& points, const arr& times_rel, double time_cut){
  arr x_cut, xDot_cut;
  eval3(x_cut, xDot_cut, NoArr, time_cut);

  arr _points(points), _times(times_rel);
  _times.prepend(0.);
  _points.prepend(x_cut);
  set(degree, _points, _times+time_cut, xDot_cut);
}

void BSpline::eval3(arr& x, arr& xDot, arr& xDDot, double t, arr& Jpoints, arr& Jtimes) const {
  uint n = ctrlPoints.d1;
  if(!!x) x.resize(n).setZero();
  if(!!xDot) xDot.resize(n).setZero();
  if(!!xDDot) xDDot.resize(n).setZero();
  if(!!Jpoints) Jpoints.resize(n, ctrlPoints.d0, n).setZero();
  if(!!Jtimes) Jtimes.resize(n, knots.N).setZero();
  uint derivative=0;
  if(!!xDot) derivative=1;
  if(!!xDDot) derivative=2;

  //-- handle out-of-interval cases
  if(t<knots(0)){
    if(!!x) x = ctrlPoints[0];
    if(!!Jpoints) for(uint i=0; i<n; i++) Jpoints(i, 0, i) = 1.;
    return;
  }
  if(t>=knots(-1)) {
    if(!!x) x = ctrlPoints[-1];
    if(!!Jpoints) for(uint i=0; i<n; i++) Jpoints(i, -1, i) = 1.;
    return;
  }

  //-- grab range of relevant knots
  int center = knots.rankInSorted(t, rai::lowerEqual<double>, true);
  center -= 1;
  int lo = center - degree;  if(lo<0) lo=0;
  int up = lo + 2*degree;  if(up>(int)knots.N-1) up=knots.N-1;
  lo = up - 2*degree;  if(lo<0) lo=0;

  //-- compute B-spline coefficients on relevant knots
  BSpline core;
  core.degree = degree;
  core.knots.referToRange(knots, {lo, up+1});
  core.calcB(t, derivative, (!!Jtimes));

  //-- multiply coefficients with control points
  arr Jtmp2;
  double b=0., bd=0., bdd=0.;
  for(uint j=0; j<core.B.d0; j++) {
    b = core.B(j, degree);
    if(derivative>=1) bd = core.Bdot(j, degree);
    if(derivative>=2) bdd = core.Bddot(j, degree);
    if(lo+j>=ctrlPoints.d0) { CHECK_ZERO(b, 1e-4, ""); continue; }
    // x += b * ctrlPoints[lo+j]; //following is faster:
    if(!!x) for(uint i=0;i<n;i++) x.elem(i) += b*ctrlPoints(lo+j,i);
    if(!!xDot) for(uint i=0;i<n;i++) xDot.elem(i) += bd*ctrlPoints(lo+j,i);
    if(!!xDDot) for(uint i=0;i<n;i++) xDDot.elem(i) += bdd*ctrlPoints(lo+j,i);
    if(!!Jpoints) {
      for(uint i=0; i<n; i++) Jpoints(i, lo+j, i) = b;
    }
    if(!!Jtimes) {
      Jtmp2.resize(n, knots.N).setZero();
      Jtmp2.setMatrixBlock(ctrlPoints[lo+j] * ~core.JBtimes(j, degree, {}), 0, lo);
      Jtimes += Jtmp2;
    }
  }
}

arr BSpline::eval(double t, uint derivative) const {
  arr x;
  if(derivative==0) eval3(x, NoArr, NoArr, t);
  else if(derivative==1) eval3(NoArr, x, NoArr, t);
  else if(derivative==2) eval3(NoArr, NoArr, x, t);
  else NIY;
  return x;
}

arr BSpline::eval(const arr& sampleTimes, uint derivative) const {
  arr f(sampleTimes.N, ctrlPoints.d1);
  if(derivative==0){ for(uint i=0; i<sampleTimes.N; i++) eval3(f[i].noconst(), NoArr, NoArr, sampleTimes(i)); }
  else if(derivative==1){ for(uint i=0; i<sampleTimes.N; i++) eval3(NoArr, f[i].noconst(), NoArr, sampleTimes(i)); }
  else if(derivative==2){ for(uint i=0; i<sampleTimes.N; i++) eval3(NoArr, NoArr, f[i].noconst(), sampleTimes(i)); }
  else NIY;
  return f;
}


// arr BSpline::getGridBasis(uint T) {
//   BSpline core;
//   core.knots.referTo(knots);
//   return core.getBmatrix(::range(0., 1., T), degree);
// }

void BSpline::append(const arr& points, const arr& times_rel, bool inside) {
  CHECK_EQ(points.nd, 2, "");
  CHECK_EQ(points.d0, times_rel.N, "");

  CHECK_GE(times_rel.first(), 0., "append needs to be in relative time, always with _times.first()>=0.");
  if(!times_rel.first()) {
    CHECK_LE(maxDiff(ctrlPoints[-1], points[0]), 1e-10, "when appending with _times.first()=0., the first point needs to be identical to the previous last, making this a double knot");
  }

  //remember end time
  double Tend = knots.last();

  //remove tails
  if(inside) {
    ctrlPoints.resizeCopy(ctrlPoints.d0-degree/2, ctrlPoints.d1);
    knots.resizeCopy(knots.N-1-2*(degree/2));
  } else {
    knots.resizeCopy(knots.N-1-(degree/2));
  }

  //append things:
  ctrlPoints.append(points);
  knots.append(times_rel+Tend);
  if(!(degree%2)) {
    arr tmp = knots;
    for(uint i=knots.N-1; i>=knots.N-times_rel.N; i--) {
      knots(i) = .5*(tmp(i-1) + tmp(i));
    }
  }

  //append tails;
  for(uint i=0; i<degree/2; i++) ctrlPoints.append(points[-1]);
  knots.append(times_rel(-1)+Tend, 1+2*(degree/2)); //multiple

  CHECK_EQ(ctrlPoints.d0, knots.N-degree-1, "");
}

arr BSpline::getPoints() {
  arr pts = ctrlPoints;
  //remove tail and head
  pts.delRows(0, degree/2);
  pts.resizeCopy(pts.d0-degree/2, pts.d1);
  return pts;
}

void BSpline::doubleKnot(uint t) {
  ctrlPoints.insRows(t + degree/2);
  ctrlPoints[t+degree/2] = ctrlPoints[t+degree/2-1];

  knots.insert(t+degree+1, knots(t+degree));
}

void BSpline::setDoubleKnotVel(int t, const arr& vel) {
//  CHECK_EQ(degree, 2, "setDoubleKnot only implemented for degree=2");
  arr a=ctrlPoints[t+degree/2];
  arr b=ctrlPoints[t+degree/2+1];
  CHECK(maxDiff(a, b)<1e-10, "this is not a double knot!");
  if(degree==2) {
    a -= vel/degree*(knots(t+degree+1)-knots(t+degree));
    b += vel/degree*(knots(t+degree+2)-knots(t+degree+1));
  } else if(degree==3) {
    a -= vel/degree*(knots(t+degree)-knots(t+degree-1));
    b += vel/degree*(knots(t+degree+2)-knots(t+degree+1));
  } else NIY;
}

//==============================================================================

void CubicPiece::set(const arr& x0, const arr& v0, const arr& x1, const arr& v1, double tau) {
  double tau2 = tau*tau, tau3 = tau*tau2;
  d = x0;
  c = v0;
  b = 1./tau2 * (3.*(x1-x0) - tau*(v1+2.*v0));
  a = 1./tau3 * (-2.*(x1-x0) + tau*(v1+v0));
}

void CubicPiece::eval(arr& x, arr& xDot, arr& xDDot, double t) const {
  double t2=t*t, t3=t*t2;
  if(!!x) {
    x = d;
    x += t*c;
    x += t2*b;
    x += t3*a;
  }
  if(!!xDot) {
    xDot = c;
    xDot += (2.*t)*b;
    xDot += (3.*t2)*a;
  }
  if(!!xDDot) {
    xDDot = 2.*b;
    xDDot += (6.*t)*a;
  }
}

arr CubicPiece::eval(double t, uint diff) {
  if(diff==3) {
    return 6.*a;
  } else {
    NIY;
  }
  return {};
}

void CubicSpline::set(const arr& pts, const arr& vels, const arr& _times) {
  CHECK_GE(_times.N, 2, "need at least 2 knots");
  times = _times;
  uint K=pts.d0-1;
  pieces.resize(K);
  for(uint k=0; k<K; k++) {
    pieces(k).set(pts[k], vels[k], pts[k+1], vels[k+1], times(k+1)-times(k));
  }
}

void CubicSpline::append(const arr& pts, const arr& vels, const arr& _times) {
  CHECK_GE(_times(0), 1e-6, "for appending, first time needs to be greater zero");

  //current end state
  arr x, xDot;
  pieces(-1).eval(x, xDot, NoArr, times(-1)-times(-2));

  //new times
  double Tend = times.last();
  times.append(_times+Tend);

  uint K0=pieces.N;
  uint K=pts.d0;
  pieces.resizeCopy(K0+K);
  pieces(K0+0).set(x, xDot, pts[0], vels[0], _times(0));
  for(uint k=1; k<K; k++) {
    pieces(K0+k).set(pts[k-1], vels[k-1], pts[k], vels[k], _times(k)-_times(k-1));
  }
}

uint CubicSpline::getPiece(double t) const {
  CHECK_GE(times.N, 2, "spline is empty");
  if(t<times(0)) return 0;
  if(t>times(-1)) return pieces.N-1;
  uint k = times.rankInSorted(t, rai::lowerEqual<double>, false);
  if(k) k--;
  if(k>=pieces.N-1) k=pieces.N-1;
  return k;
}

void CubicSpline::eval(arr& x, arr& xDot, arr& xDDot, double t) const {
  CHECK_GE(times.N, 2, "spline is empty");
  if(t<times(0)) {
    pieces(0).eval(x, xDot, xDDot, 0.);
    if(!!xDDot) {
      CHECK_ZERO(absMax(xDot), 1e-6, "don't query a cubic spline at neg time for non-zero start velocity");
      xDDot.setZero();
    }
    return;
  }
  if(t>times(-1)) {
    pieces(-1).eval(x, xDot, xDDot, times(-1)-times(-2));
    if(!!xDDot) {
      CHECK_ZERO(absMax(xDot), 1e-6, "don't query a cubic spline after final time for non-zero final velocity");
      xDDot.setZero();
    }
    return;
  }

  uint k = times.rankInSorted(t, rai::lowerEqual<double>, false);
  if(k<times.N) { CHECK_LE(t, times(k), ""); }
  else { CHECK_GE(t, times.last(), ""); }
  if(k) k--;
  if(k>=pieces.N-1) k=pieces.N-1;
  //    CHECK_GE(t, times(k), "");
  //    cout <<"t: " <<t <<" k: " <<k <<' ' <<times <<endl;

  pieces(k).eval(x, xDot, xDDot, t-times(k));
}

arr CubicSpline::eval(double t, uint diff) const {
  arr ret;
  if(diff==0) eval(ret, NoArr, NoArr, t);
  else if(diff==1) eval(NoArr, ret, NoArr, t);
  else if(diff==2) eval(NoArr, NoArr, ret, t);
  else { uint k=getPiece(t); ret = pieces(k).eval(t-times(k), diff); }
  return ret;
}

arr CubicSpline::eval(const arr& T, uint diff) const {
  arr x(T.N, pieces.first().d.N);
  for(uint i=0; i<T.N; i++) x[i] = eval(T(i), diff);
  return x;
}

arr CubicSplineLeapCost(const arr& x0, const arr& v0, const arr& x1, const arr& v1, double tau, const arr& tauJ) {
  arr D = (x1-x0) - (.5*tau)*(v0+v1);
  if(tauJ.N) {
    if(!D.jac) D.J().sparse().resize(v0.N, tauJ.d1, 0);
    D.J() -= .5 * (v0+v1).noJ() * tauJ;
  }

  arr V = v1 - v0;

  double s12 = sqrt(12.);

  arr tilD = (s12 * pow(tau, -1.5)) * D;
  if(tauJ.N) tilD.J() += (s12 * (-1.5) * pow(tau, -2.5)) * D.noJ() * tauJ;

  arr tilV = pow(tau, -0.5) * V;
  if(tauJ.N) {
    if(!tilV.jac) tilV.J().sparse().resize(v0.N, tauJ.d1, 0);
    tilV.J() += ((-0.5) * pow(tau, -1.5)) * V.noJ() * tauJ;
  }

  arr y;
  y.setBlockVector(tilD, tilV);
  return y;
}

arr CubicSplineMaxJer(const arr& x0, const arr& v0, const arr& x1, const arr& v1, double tau, const arr& tauJ) {
  //jerk is 6a
  double tau2 = tau*tau, tau3 = tau*tau2, tau4 = tau2*tau2;
  arr a6 = 6./tau3 * (-2.*(x1-x0) + tau*(v1+v0));
  if(tauJ.N) {
    a6.J() += (36./tau4 * (x1-x0)) * tauJ;
    a6.J() += (-12./tau3 * (v1+v0)) * tauJ;
  }

  uint n=x0.N;
  arr y = zeros(2*n);
  if(a6.jac) y.J().sparse().resize(y.N, a6.jac->d1, 0);
  y.setVectorBlock(a6, 0*n);
  y.setVectorBlock(-a6, 1*n);
  return y;
}

arr CubicSplineAcc0(const arr& x0, const arr& v0, const arr& x1, const arr& v1, double tau, const arr& tauJ) {
  //acceleration is 6a t + 2b
  double tau2 = tau*tau, tau3 = tau*tau2;
  arr b2 = 2./tau2 * (3.*(x1-x0) - tau*(v1+2.*v0));
  if(tauJ.N) {
    b2.J() += -12./tau3 * (x1-x0) * tauJ;
    b2.J() -= -2./tau2 * (v1+2.*v0) * tauJ;
  }
  return b2;
}

arr CubicSplineAcc1(const arr& x0, const arr& v0, const arr& x1, const arr& v1, double tau, const arr& tauJ) {
  //acceleration is 6a t + 2b
  double tau2 = tau*tau, tau3 = tau*tau2;
  //  arr d = x0;
  //  arr c = v0;
  arr b2 = 2./tau2 * (3.*(x1-x0) - tau*(v1+2.*v0));
  if(tauJ.N) {
    b2.J() += -12./tau3 * (x1-x0) * tauJ;
    b2.J() -= -2./tau2 * (v1+2.*v0) * tauJ;
  }
  arr a6_tau = 6./tau2 * (-2.*(x1-x0) + tau*(v1+v0));
  if(tauJ.N) {
    a6_tau.J() -= -24./tau3 * (x1-x0) * tauJ;
    a6_tau.J() += -6./tau2 * (v1+v0) * tauJ;
  }
  return a6_tau + b2;
}

arr CubicSplineMaxAcc(const arr& x0, const arr& v0, const arr& x1, const arr& v1, double tau, const arr& tauJ) {
  //acceleration is 6a t + 2b
  double tau2 = tau*tau, tau3 = tau*tau2;
  //  arr d = x0;
  //  arr c = v0;
  arr b2 = 2./tau2 * (3.*(x1-x0) - tau*(v1+2.*v0));
  if(tauJ.N) {
    b2.J() += -12./tau3 * (x1.noJ()-x0.noJ()) * tauJ;
    b2.J() -= -2./tau2 * (v1.noJ()+2.*v0.noJ()) * tauJ;
  }
  arr a6_tau = 6./tau2 * (-2.*(x1-x0) + tau*(v1+v0));
  if(tauJ.N) {
    a6_tau.J() -= -24./tau3 * (x1.noJ()-x0.noJ()) * tauJ;
    a6_tau.J() += -6./tau2 * (v1.noJ()+v0.noJ()) * tauJ;
  }

  uint d=x0.N;
  arr y = zeros(4*d);
  if(b2.jac) y.J().sparse().resize(y.N, b2.jac->d1, 0);
  y.setVectorBlock(b2, 0*d);
  y.setVectorBlock(-b2, 1*d);
  y.setVectorBlock(b2 + a6_tau, 2*d);
  y.setVectorBlock(-b2 - a6_tau, 3*d);
  return y;
}

arr CubicSplineMaxVel(const arr& x0, const arr& v0, const arr& x1, const arr& v1, double tau, const arr& tauJ) {
  //acc is 6a t + 2b; with root at t=-b/3a
  //velocity is 3a t^2 + 2b t + c; at root is -b^2 / 3a + c

#if 1
  double tau2 = tau*tau;
  //  arr d = x0;
  arr c = v0;
  arr b = (3.*(x1-x0) - tau*(v1+2.*v0));
  if(tauJ.N) {
    b.J() -= (v1+2.*v0) * tauJ;
  }
  arr a = (-2.*(x1-x0) + tau*(v1+v0));
  if(tauJ.N) {
    a.J() += (v1+v0) * tauJ;
  }
  arr t=-tau*b.noJ()/(3.*a.noJ());
  //indicators for each dimension
  arr iv0=zeros(t.N), iv1=zeros(t.N), ivm=zeros(t.N);
  for(uint i=0; i<t.N; i++) {
    if(t(i)<=0) iv0(i)=1.;
    else if(t(i)>=tau) iv1(i)=1.;
    else ivm(i)=1.;
  }
  arr vmax = c + (1./tau) * (b + 3./4.*a);
  if(tauJ.N) {
    vmax.J() -= (1./tau2) * (b + 3./4.*a) * tauJ;
  }

//  vmax = iv0%v0;
//  vmax += iv1%v1;
//  vmax += ((1./(3.*tau)) * ((ivm%b%b)/a) + c);
//  if(tauJ.N){
//    vmax.J() -= (1./(3.*tau2)) * ((ivm%b%b)/a) * tauJ;
//  }
#endif

  uint n=x0.N;
  arr y(4*n);
  y.setZero();
  if(v0.jac) y.J().sparse().resize(y.N, v0.jac->d1, 0);
  else if(vmax.jac) y.J().sparse().resize(y.N, vmax.jac->d1, 0);
  y.setVectorBlock(v0, 0*n);
  y.setVectorBlock(-v0, 1*n);
  y.setVectorBlock(vmax, 2*n);
  y.setVectorBlock(-vmax, 3*n);
  return y;
}

void CubicSplinePosVelAcc(arr& pos, arr& vel, arr& acc, double trel, const arr& x0, const arr& v0, const arr& x1, const arr& v1, double tau, const arr& tauJ) {
  //position at time t:
  // a t^3 + b t^2 + c t + d

  CHECK_GE(trel, 0., "");
  CHECK_LE(trel, 1., "");

  double tau2 = tau*tau;
  double tau3 = tau2*tau;
  arr d = x0;
  arr c = v0;
#if 0
  arr b = 1./tau2 * (3.*(x1-x0) - tau*(v1+2.*v0));
  if(tauJ.N) {
    b.J() += -6./tau3 * (x1.noJ()-x0.noJ()) * tauJ;
    b.J() -= -1./tau2 * (v1.noJ()+2.*v0.noJ()) * tauJ;
  }
  arr a = 1./tau3 * (-2.*(x1-x0) + tau*(v1+v0));
  if(tauJ.N) {
    a.J() -= -6./tau4 * (x1.noJ()-x0.noJ()) * tauJ;
    a.J() += -2./tau3 * (v1.noJ()+v0.noJ()) * tauJ;
  }
#endif
  arr c_tau = tau*c;
  if(tauJ.N) { if(c_tau.jac) c_tau.J() += (c) * tauJ; else c_tau.J() = c*tauJ; }

  arr b_tau2 = (3.*(x1-x0) - tau*(v1+2.*v0));
  if(tauJ.N) b_tau2.J() -= (v1.noJ()+2.*v0.noJ()) * tauJ;

  arr b_tau1 = 1./tau * b_tau2;
  if(tauJ.N) b_tau1.J() += (-1./tau2) * b_tau2.noJ() * tauJ;

  arr b_tau0 = 1./tau2 * b_tau2;
  if(tauJ.N) b_tau0.J() += (-2./tau3) * b_tau2.noJ() * tauJ;

  arr a_tau3 = (-2.*(x1-x0) + tau*(v1+v0));
  if(tauJ.N) a_tau3.J() += (v1+v0) * tauJ;

  arr a_tau2 = 1./tau * a_tau3;
  if(tauJ.N) a_tau2.J() += (-1./tau2) * a_tau3.noJ() * tauJ;

  arr a_tau1 = 1./tau2 * a_tau3;
  if(tauJ.N) a_tau1.J() += (-2./tau3) * a_tau3.noJ() * tauJ;

  //arr a_tau0 = 1./tau3 * a_tau3;
  //if(tauJ.N) a_tau0.J() += (-3./tau4) * a_tau3.noJ() * tauJ;

  if(!!pos) pos = (trel*trel*trel)*a_tau3 + (trel*trel)*b_tau2 + trel*c_tau + d;
  if(!!vel) vel = (3.*trel*trel)*a_tau2 + (2.*trel)*b_tau1 + c;
  if(!!acc) acc = (6.*trel)*a_tau1 + (2.)*b_tau0;
#if 0
  if(!!pos) pos = (t*t*t)*a + (t*t)*b + t*c + d;
  if(!!vel) vel = (3.*t*t)*a + (2.*t)*b + c;
  if(!!acc) acc = (6.*t)*a + (2.)*b;
#endif
}

arr BSpline_path2ctrlPoints(const arr& path, uint numCtrlPoints, uint degree, bool startDuplicates, bool endDuplicates){
  CHECK_EQ(path.nd, 2, "");
  CHECK(path.d0, "");
  BSpline S;
  S.setKnots(degree, ::range(0., 1., numCtrlPoints-1));
  arr B = S.getBmatrix(::range(0., 1., path.d0-1), startDuplicates, endDuplicates);

  arr Binv = pseudoInverse(B);
  arr ctrlPoints = Binv * path;
  return ctrlPoints;
}

} //namespace rai
