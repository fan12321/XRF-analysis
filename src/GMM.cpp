#include "GMM.h"
#include <cmath>
#include <QDebug>
#include <QtConcurrent>
#include <QFuture>

GMM::GMM(std::vector<float>& arr) : 
    X(arr), 
    weights({}), 
    means({}), 
    sigmas({}), 
    vars({}), 
    cuts({})
{
    weightsList.resize(_maxNumOfGaussians);
    meansList.resize(_maxNumOfGaussians);
    sigmasList.resize(_maxNumOfGaussians);
    varsList.resize(_maxNumOfGaussians);

    meansVisList.resize(_maxNumOfGaussians);
    sigmasVisList.resize(_maxNumOfGaussians);
    logLikelihoodList.resize(_maxNumOfGaussians);

    aicList.resize(_maxNumOfGaussians);
    bicList.resize(_maxNumOfGaussians);
}

float GMM::gaussian(float x, float mean, float var) {
    const float PI = 3.14159265358979323846;
    return (1.0 / sqrt(2.0 * PI * var)) * exp(-(pow(x - mean, 2) / (2.0 * var)));
}

void GMM::kmeans1D(const std::vector<float>& X, std::vector<float>& means, int K, std::vector<int>& labels) {
    int N = X.size();
    labels.assign(N, 0);

    // Repeat until stable
    for (int iter = 0; iter < 100; iter++) {
        // Assign step
        for (int i = 0; i < N; i++) {
            float bestDist = abs(X[i] - means[0]);
            int bestK = 0;
            for (int k = 1; k < K; k++) {
                float dist = abs(X[i] - means[k]);
                if (dist < bestDist) {
                    bestDist = dist;
                    bestK = k;
            }
            }
            labels[i] = bestK;
        }

        // Update step
        std::vector<float> sum(K, 0.0), count(K, 0.0);
        for (int i = 0; i < N; i++) {
            sum[labels[i]] += X[i];
            count[labels[i]] += 1.0;
        }

        bool converged = true;
        for (int k = 0; k < K; k++) {
            if (count[k] > 0) {
                float newMean = sum[k] / count[k];
                if (fabs(newMean - means[k]) > 1e-6)
                    converged = false;
                means[k] = newMean;
            }
        }

        if (converged) break;
    }
}

void GMM::gmm(int numberOfComponents) {
    _max = X[0] - 1.0;
    _min = X[0] + 1.0;
    for (auto val: X) {
        _max = (_max > val)? _max : val;
        _min = (_min < val)? _min : val;
    }

    int N = X.size();
    int K = numberOfComponents;

    // K-Means Initialization
    std::vector<float> mean(K, 0.0);
    for (int i=0; i<K; i++) mean[i] = static_cast<float>(i) + _min; // naive guess

    std::vector<int> labels;
    kmeans1D(X, mean, K, labels);

    // Compute initial variances + mixing weights
    std::vector<float> var(K, 1.0);
    std::vector<float> pi(K, 0.0);
    for (int k = 0; k < K; k++) {
        float sum = 0.0, count = 0.0;
        for (int i = 0; i < N; i++) {
            if (labels[i] == k) {
                sum += pow(X[i] - mean[k], 2);
                count += 1.0;
            }
        }
        if (count > 0) var[k] = sum / count;
        if (var[k] < 1e-6) var[k] = 1e-6;
        pi[k] = count / N;
    }

    // EM variables
    std::vector<std::vector<float>> gamma(N, std::vector<float>(K));
    float prev_log_likelihood = -1e18;
    const float EPS = 1e-6;

    float log_likelihood;
    // ---------- EM Algorithm ----------
    for (int iter = 0; iter < 100; iter++) {

        // E-Step
        for (int n = 0; n < N; n++) {
            float denom = 0.0;
            for (int k = 0; k < K; k++)
                denom += pi[k] * gaussian(X[n], mean[k], var[k]);
            if (denom < 1e-12) denom = 1e-12;

            for (int k = 0; k < K; k++)
                gamma[n][k] = pi[k] * gaussian(X[n], mean[k], var[k]) / denom;
        }

        // M-Step
        for (int k = 0; k < K; k++) {
            float Nk = 0.0;
            for (int n = 0; n < N; n++)
                Nk += gamma[n][k];
            if (Nk < 1e-12) continue;

            float newMean = 0.0;
            for (int n = 0; n < N; n++)
                newMean += gamma[n][k] * X[n];
            newMean /= Nk;

            float newVar = 0.0;
            for (int n = 0; n < N; n++)
                newVar += gamma[n][k] * pow(X[n] - newMean, 2);
            newVar /= Nk;
            if (newVar < 1e-6) newVar = 1e-6;

            mean[k] = newMean;
            var[k] = newVar;
            pi[k] = Nk / N;
        }

        // Log-likelihood
        log_likelihood = 0.0;
        for (float x : X) {
            float sum_prob = 0.0;
            for (int k = 0; k < K; k++)
                sum_prob += pi[k] * gaussian(x, mean[k], var[k]);
            log_likelihood += log(sum_prob + 1e-12);
        }

        if (fabs(log_likelihood - prev_log_likelihood) < EPS) {
            qDebug() << "\nEM Converged after " << iter << " iterations.\n";
            break;
        }
        prev_log_likelihood = log_likelihood;
    }

    // Output final parameters
    std::vector<size_t> idx(K);
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(), [&mean](float i1, float i2) {
        return mean[i1] < mean[i2];
    });
    for (int k: idx) {
        // weights.push_back(pi[k]);
        // means.push_back(mean[k]);
        // sigmas.push_back(sqrt(var[k]));
        // vars.push_back(var[k]);
        weightsList[numberOfComponents-1].push_back(pi[k]);
        meansList[numberOfComponents-1].push_back(mean[k]);
        sigmasList[numberOfComponents-1].push_back(sqrt(var[k]));
        varsList[numberOfComponents-1].push_back(var[k]);

        mean[k] = (mean[k] - _min) * _scale / (_max - _min);
        var[k] = var[k] * (_scale * _scale / ((_max - _min) * (_max - _min)));

        // qDebug() << "\nComponent " << k << ":\n";
        // qDebug() << " Weight = " << pi[k];
        // qDebug() << " Mean   = " << mean[k];
        // qDebug() << " Var    = " << var[k];

        meansVisList[numberOfComponents-1].push_back(mean[k]);
        sigmasVisList[numberOfComponents-1].push_back(sqrt(var[k]));

    }
    aicList[numberOfComponents-1] = -2.0 * log_likelihood + 2 * (3*numberOfComponents - 1);
    bicList[numberOfComponents-1] = -2.0 * log_likelihood + (3*numberOfComponents - 1) * std::log10(N * 1.0);

    qDebug() << "bic " << numberOfComponents << ": " << bicList[numberOfComponents-1];
    qDebug() << "aic " << numberOfComponents << ": " << aicList[numberOfComponents-1];
    logLikelihoodList[numberOfComponents-1] = log_likelihood;
}

void GMM::calculateCuts(int numberOfGaussians) {
    auto const gaussVal = [this, numberOfGaussians](float x, int gaussId) {
        return weightsList[numberOfGaussians-1][gaussId] * gaussian(x, meansList[numberOfGaussians-1][gaussId], varsList[numberOfGaussians-1][gaussId]);
    };

    for (int i=0; i<numberOfGaussians-1; i++) {
        float leftX = meansList[numberOfGaussians-1][i];
        float rightX = meansList[numberOfGaussians-1][i+1];
        // qDebug() << "find intersection " << i << ", between " << leftX << " and " << rightX;
        if (
            (gaussVal(leftX, i) - gaussVal(leftX, i+1)) * (gaussVal(rightX, i) - gaussVal(rightX, i+1)) < 0.0
        ) {
            // TODO find intersection
            float midX, diff;
            int search = 0;
            while (true) {
                search++; // max 10 binary search
                midX = (leftX + rightX) * 0.5;
                diff = gaussVal(midX, i) - gaussVal(midX, i+1);
                if (std::fabs(diff) < 1e-7 || search == 10) {
                    // qDebug() << "result: " << midX << ", diff: " << diff << "\n";
                    cuts.push_back(midX);
                    break;
                }
                else if (diff < 0.0) {
                    // left part
                    rightX = midX;
                }
                else {
                    leftX = midX;
                }
            }
        }
    }
}

int GMM::bestFit() {
    std::vector<QFuture<void>> futureVector(_maxNumOfGaussians);
    for (int numOfGaussians=1; numOfGaussians <= _maxNumOfGaussians; numOfGaussians++) {
        auto future = QtConcurrent::run([this, numOfGaussians]() {
            gmm(numOfGaussians);
        });
        futureVector[numOfGaussians-1] = future;
    }
    for (int numOfGaussians=1; numOfGaussians <= _maxNumOfGaussians; numOfGaussians++) {
        futureVector[numOfGaussians-1].waitForFinished();
    }

    float localMin = 0.0;
    int bestComponent = 1;
    for (int numOfGaussians=2; numOfGaussians<=5; numOfGaussians++) {
        float improve = aicList[numOfGaussians-1] - aicList[numOfGaussians-2];
        if (improve < localMin) {
            bestComponent = numOfGaussians;
            localMin = improve;
        }
    }
    return bestComponent;
}