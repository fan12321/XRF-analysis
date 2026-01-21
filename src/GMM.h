#pragma once
#include <vector>

class GMM
{
public:
    GMM(std::vector<float>&);
    float gaussian(float x, float mean, float var);
    void kmeans1D(const std::vector<float>& X, std::vector<float>& means, int K, std::vector<int>& labels);

    void gmm(int numberOfComponents=2);
    int bestFit();
    void calculateCuts(int numberOfComponents);
    std::vector<float>& getCuts() { return cuts; };
    float getLogLikelihood(int numComponents) { return logLikelihoodList[numComponents-1]; };

    std::vector<float>& getWeights(int numberOfComponents) { return weightsList[numberOfComponents-1]; };
    std::vector<float>& getMeans(int numberOfComponents) { return meansVisList[numberOfComponents-1]; };
    std::vector<float>& getSigmas(int numberOfComponents) { return sigmasVisList[numberOfComponents-1]; };

private:
    std::vector<float> X;

    // for each GMM of different number of componenet
    // for visualization (scaled to 0 - 127)
    std::vector<float> meansVis;
    std::vector<float> sigmasVis;

    // actual result
    std::vector<float> weights;
    std::vector<float> means;
    std::vector<float> sigmas;
    std::vector<float> vars;

    // GMM of different n_componenets
    std::vector<std::vector<float>> weightsList;
    std::vector<std::vector<float>> meansList;
    std::vector<std::vector<float>> sigmasList;
    std::vector<std::vector<float>> varsList;

    std::vector<std::vector<float>> meansVisList;
    std::vector<std::vector<float>> sigmasVisList;

    std::vector<float> aicList;
    std::vector<float> bicList;

    std::vector<float> logLikelihoodList;

    // cut between pair of gaussians
    std::vector<float> cuts;

    float _scale {128.0};
    float _min;
    float _max;

    int _maxNumOfGaussians {5};
};
