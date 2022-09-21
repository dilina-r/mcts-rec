#pragma once

#include <random>

class User {
public:
	int group;
	int num_groups;
	double *mu;
	double *sigma2;
	std::random_device rd{};
	std::mt19937 gen{rd()};
	std::normal_distribution<> gaussian {0.0,1.0};

	void create(int group, int num_groups, double *mu, double *sigma2) {
		this->group = group;
		this->num_groups = num_groups;
		this->mu = mu;
		this->sigma2 = sigma2;
	}

	double rating(int group, int item) {
		return mu[group] + gaussian(gen)*sqrt(sigma2[group]);
	}
};