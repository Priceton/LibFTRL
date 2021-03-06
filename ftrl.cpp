#include "ftrl.h"


FtrlChunk::FtrlChunk(string data_name, FtrlInt id) {
    l = 0, nnz = 0;
    chunk_id = id;
    file_name = data_name+".bin."+to_string(id);
}

void FtrlChunk::write() {
    FILE *f_bin = fopen(file_name.c_str(), "wb");
    if (f_bin == nullptr)
        cout << "Error" << endl;


    fwrite(labels.data(), sizeof(FtrlFloat), l, f_bin);
    fwrite(nnzs.data(), sizeof(FtrlInt), l+1, f_bin);
    fwrite(R.data(), sizeof(FtrlFloat), l, f_bin);
    fwrite(nodes.data(), sizeof(Node), nnz, f_bin);
    fclose(f_bin);
}

void FtrlChunk::read() {
    FILE *f_tr = fopen(file_name.c_str(), "rb");
    if (f_tr == nullptr)
        cout << "Error" << endl;

    labels.resize(l);
    R.resize(l);
    nodes.resize(nnz);
    nnzs.resize(l+1);

    fread(labels.data(), sizeof(FtrlFloat), l, f_tr);
    fread(nnzs.data(), sizeof(FtrlInt), l+1, f_tr);
    fread(R.data(), sizeof(FtrlFloat), l, f_tr);
    fread(nodes.data(), sizeof(Node), nnz, f_tr);
    fclose(f_tr);
}

void FtrlChunk::clear() {
    labels.clear();
    nodes.clear();
    R.clear();
    nnzs.clear();
}

void FtrlData::split_chunks() {
    string line;
    ifstream fs(file_name);
    
    FtrlInt i = 0, chunk_id = 0;
    FtrlChunk chunk(file_name, chunk_id);
    nr_chunk++;

    chunk.nnzs.push_back(i);

    while (getline(fs, line)) {
        FtrlFloat label = 0;
        istringstream iss(line);

        l++;
        chunk.l++;

        iss >> label;
        label = (label>0)? 1:-1;
        chunk.labels.push_back(label);

        FtrlInt idx = 0;
        FtrlFloat val = 0;

        char dummy;
        FtrlFloat r = 0;
        FtrlInt max_nnz = 0;
        while (iss >> idx >> dummy >> val) {
            i++;
            max_nnz++;
            if (n < idx+1) {
                n = idx+1; 
            }
            chunk.nodes.push_back(Node(idx, val));
            r += val*val;
        }
        chunk.nnzs.push_back(i);
        chunk.R.push_back(1/sqrt(r));
        if (i > chunk_size) {

            chunk.nnz = i;
            chunk.write();
            chunk.clear();

            chunks.push_back(chunk);

            i = 0;
            chunk_id++;
            chunk = FtrlChunk(file_name, chunk_id);
            chunk.nnzs.push_back(i);
            nr_chunk++;
        }
    }

    chunk.nnz = i;
    chunk.write();
    chunk.clear();

    chunks.push_back(chunk);
}

void FtrlData::print_data_info() {
    cout << "Data: " << file_name << "\t";
    cout << "#features: " << n << "\t";
    cout << "#instances: " << l << "\t";
    cout << "#chunks " << nr_chunk << "\t";
    cout << endl;
}

void FtrlProblem::save_model(string model_path) {
    ofstream f_out(model_path);
    f_out << "norm " << param->normalized << endl;
    f_out << "n " << data->n << endl;

    FtrlFloat *wa = w.data();
    for (FtrlFloat j = 0; j < data->n; j++, wa++)
    {
        f_out << "w" << j << " " << *wa << endl;
    }
    f_out.close();
}

FtrlLong FtrlProblem::load_model(string model_path) {

    ifstream f_in(model_path);

    string dummy;
    FtrlLong nr_feature;

    f_in >> dummy >> dummy >> dummy >> nr_feature;
    w.resize(nr_feature);


    FtrlFloat *ptr = w.data();
    for(FtrlLong j = 0; j < nr_feature; j++, ptr++)
    {
        f_in >> dummy;
        f_in >> *ptr;
    }
    return nr_feature;
}

void FtrlProblem::initialize() {
    w.resize(data->n, 0);
    z.resize(data->n, 0);
    n.resize(data->n, 0);
    f.resize(data->n, 0);
    t = 0;
    tr_loss = 0.0, va_loss = 0.0, fun_val = 0.0, gnorm = 0.0;
    FtrlInt nr_chunk = data->nr_chunk;
    for (FtrlInt chunk_id = 0; chunk_id < nr_chunk; chunk_id++) {

        FtrlChunk chunk = data->chunks[chunk_id];

        chunk.read();

        for (FtrlInt i = 0; i < chunk.l; i++) {

            for (FtrlInt s = chunk.nnzs[i]; s < chunk.nnzs[i+1]; s++) {
                Node x = chunk.nodes[s];
                FtrlInt idx = x.idx;
                f[idx]++;
            }
        }
        chunk.clear();
    }
    for (FtrlInt j = 0; j < data->n; j++) {
        if (param->freq)
            f[j]  = 1;
        else
            f[j]  = 1/f[j];
    }
    start_time = omp_get_wtime();
}

void FtrlProblem::print_header_info() {
    cout.width(4);
    cout << "iter";
    if (param->verbose) {
    cout.width(13);
    cout << "fun_val";
    cout.width(13);
    cout << "reg";
    cout.width(13);
    cout << "|grad|";
    cout.width(13);
    cout << "tr_logloss";
    }
    if(!test_data->file_name.empty()) {
        cout.width(13);
        cout << "va_logloss";
        cout.width(13);
        cout << "va_auc";
    }
    cout.width(13);
    cout << "time";
    cout << endl;
}
void FtrlProblem::print_epoch_info() {
    cout.width(4);
    cout << t+1;
    if (param->verbose) {
        cout.width(13);
        cout << scientific << setprecision(3) << fun_val;
        cout.width(13);
        cout << scientific << setprecision(3) << reg;
        cout.width(13);
        cout << scientific << setprecision(3) << gnorm;
        cout.width(13);
        cout << fixed << setprecision(5) << tr_loss;
    }
    if (!test_data->file_name.empty()) {
        cout.width(13);
        cout << fixed << setprecision(5) << va_loss;
        cout.width(13);
        cout << fixed << setprecision(5) << va_auc;
    }
    cout.width(13);
    cout << fixed << setprecision(5) << omp_get_wtime() - start_time;
    cout << endl;
} 


void FtrlProblem::validate() {
    FtrlInt nr_chunk = test_data->nr_chunk, global_i = 0;
    FtrlFloat local_va_loss = 0.0;
    vector<FtrlFloat> va_labels(test_data->l, 0), va_scores(test_data->l, 0), va_orders(test_data->l, 0);
    for (FtrlInt chunk_id = 0; chunk_id < nr_chunk; chunk_id++) {

        FtrlChunk chunk = test_data->chunks[chunk_id];
        chunk.read();

#pragma omp parallel for schedule(static) reduction(+: local_va_loss)
        for (FtrlInt i = 0; i < chunk.l; i++) {

            FtrlFloat y = chunk.labels[i], wTx = 0;

            for (FtrlInt s = chunk.nnzs[i]; s < chunk.nnzs[i+1]; s++) {
                Node x = chunk.nodes[s];
                FtrlInt idx = x.idx;
                if (idx > data->n) {
                    continue;
                }
                FtrlFloat val = x.val;
                wTx += w[idx]*val;
            }
            va_scores[global_i+i] = wTx;
            va_orders[global_i+i] = global_i+i;
            va_labels[global_i+i] = y;

            FtrlFloat exp_m;

            if (wTx*y > 0) {
                exp_m = exp(-y*wTx);
                local_va_loss += log(1+exp_m);
            }
            else {
                exp_m = exp(y*wTx);
                local_va_loss += -y*wTx+log(1+exp_m); 
            }
        }
        global_i += chunk.l;
        chunk.clear();
    }
    va_loss = local_va_loss / test_data->l;

    sort(va_orders.begin(), va_orders.end(), [&va_scores] (FtrlInt i, FtrlInt j) {return va_scores[i] < va_scores[j];});

    FtrlFloat prev_score = va_scores[0];
    FtrlLong M = 0, N = 0;
    FtrlLong begin = 0, stuck_pos = 0, stuck_neg = 0;
    FtrlFloat sum_pos_rank = 0;

    for (FtrlInt i = 0; i < test_data->l; i++)
    {
        FtrlInt sorted_i = va_orders[i];

        FtrlFloat score = va_scores[sorted_i];

        if (score != prev_score)
        {
            sum_pos_rank += stuck_pos*(begin+begin-1+stuck_pos+stuck_neg)*0.5;
            prev_score = score;
            begin = i;
            stuck_neg = 0;
            stuck_pos = 0;
        }

        FtrlFloat label = va_labels[sorted_i];

        if (label > 0)
        {
            M++;
            stuck_pos ++;
        }
        else
        {
            N++;
            stuck_neg ++;
        }
    }
    sum_pos_rank += stuck_pos*(begin+begin-1+stuck_pos+stuck_neg)*0.5;
    va_auc = (sum_pos_rank - 0.5*M*(M+1)) / (M*N);
}

void FtrlProblem::solve_adagrad() {
    print_header_info();
    FtrlInt nr_chunk = data->nr_chunk;
    FtrlFloat l2 = param->l2, a = param->alpha, b = param->beta;
    for (t = 0; t < param->nr_pass; t++) {
    for (FtrlInt chunk_id = 0; chunk_id < nr_chunk; chunk_id++) {

        FtrlChunk chunk = data->chunks[chunk_id];

        chunk.read();

        for (FtrlInt i = 0; i < chunk.l; i++) {

            FtrlFloat y=chunk.labels[i], wTx=0;

            for (FtrlInt s = chunk.nnzs[i]; s < chunk.nnzs[i+1]; s++) {
                Node x = chunk.nodes[s];
                FtrlInt idx = x.idx;
                FtrlFloat val = x.val;
                wTx += w[idx]*val;
            }

            FtrlFloat exp_m, tmp;

            if (wTx*y > 0) {
                exp_m = exp(-y*wTx);
                tmp = exp_m/(1+exp_m);
            }
            else {
                exp_m = exp(y*wTx);
                tmp = 1/(1+exp_m);
            }

            FtrlFloat kappa = -y*tmp;

            for (FtrlInt s = chunk.nnzs[i]; s < chunk.nnzs[i+1]; s++) {
                Node x = chunk.nodes[s];
                FtrlInt idx = x.idx;
                FtrlFloat val = x.val, g = kappa*val+l2*f[idx]*w[idx];
                n[idx] += g*g;
                w[idx] -= (a/(b+sqrt(n[idx])))*g;
            }
        }
        chunk.clear();
    }
    if (param->verbose)
        fun();
    if (!test_data->file_name.empty()) {
    validate();
    }
    print_epoch_info();
    }
}

void FtrlProblem::solve_rda() {
    print_header_info();
    FtrlInt nr_chunk = data->nr_chunk;
    FtrlFloat l2 = param->l2, a = param->alpha, b = param->beta;
    for (t = 0; t < param->nr_pass; t++) {
    for (FtrlInt chunk_id = 0; chunk_id < nr_chunk; chunk_id++) {

        FtrlChunk chunk = data->chunks[chunk_id];

        chunk.read();

        for (FtrlInt i = 0; i < chunk.l; i++) {

            FtrlFloat y=chunk.labels[i], wTx=0;

            for (FtrlInt s = chunk.nnzs[i]; s < chunk.nnzs[i+1]; s++) {
                Node x = chunk.nodes[s];
                FtrlInt idx = x.idx;
                FtrlFloat val = x.val;
                wTx += w[idx]*val;
            }

            FtrlFloat exp_m, tmp;

            if (wTx*y > 0) {
                exp_m = exp(-y*wTx);
                tmp = exp_m/(1+exp_m);
            }
            else {
                exp_m = exp(y*wTx);
                tmp = 1/(1+exp_m);
            }

            FtrlFloat kappa = -y*tmp;

            for (FtrlInt s = chunk.nnzs[i]; s < chunk.nnzs[i+1]; s++) {
                Node x = chunk.nodes[s];
                FtrlInt idx = x.idx;
                FtrlFloat val = x.val, g = kappa*val;
                z[idx] += g;
                w[idx] = -z[idx] / ((b+sqrt(n[idx]))/a+l2*f[idx]);
                n[idx] += g*g;
            }
        }
        chunk.clear();
    }
    if (param->verbose)
        fun();
    if (!test_data->file_name.empty()) {
    validate();
    }
    print_epoch_info();
    }
}

void FtrlProblem::fun() {
    FtrlFloat l1 = param->l1, l2 = param->l2;
    vector<FtrlFloat> grad(data->n, 0);
    FtrlInt nr_chunk = data->nr_chunk;
    fun_val = 0.0, tr_loss = 0.0, gnorm = 0.0, reg = 0.0;
    for (FtrlInt chunk_id = 0; chunk_id < nr_chunk; chunk_id++) {

        FtrlChunk chunk = data->chunks[chunk_id];

        chunk.read();

        FtrlFloat local_tr_loss = 0.0;

#pragma omp parallel for schedule(guided) reduction(+: local_tr_loss)
        for (FtrlInt i = 0; i < chunk.l; i++) {

            FtrlFloat y=chunk.labels[i], wTx=0;

            for (FtrlInt s = chunk.nnzs[i]; s < chunk.nnzs[i+1]; s++) {
                Node x = chunk.nodes[s];
                FtrlInt idx = x.idx;
                FtrlFloat val = x.val;
                wTx += w[idx]*val;
            }

            FtrlFloat exp_m, tmp;

            if (wTx*y > 0) {
                exp_m = exp(-y*wTx);
                tmp = exp_m/(1+exp_m);
                local_tr_loss += log(1+exp_m);
            }
            else {
                exp_m = exp(y*wTx);
                tmp = 1/(1+exp_m);
                local_tr_loss += -y*wTx+log(1+exp_m); 
            }

            FtrlFloat kappa = -y*tmp;

            for (FtrlInt s = chunk.nnzs[i]; s < chunk.nnzs[i+1]; s++) {
                Node x = chunk.nodes[s];
                FtrlInt idx = x.idx;
                FtrlFloat val = x.val, g = kappa*val+l2*f[idx]*w[idx];
                grad[idx] += g;
            }
        }
        tr_loss += local_tr_loss;
        chunk.clear();
    }
    for (FtrlInt j = 0; j < data->n; j++) {
        gnorm += grad[j]*grad[j];
        reg += (l1*abs(w[j]) + 0.5*l2*w[j]*w[j]);
    }
    fun_val = tr_loss + reg;
    tr_loss /= data->l;
    gnorm = sqrt(gnorm);
}

void FtrlProblem::solve() {
    print_header_info();
    FtrlInt nr_chunk = data->nr_chunk;
    FtrlFloat l1 = param->l1, l2 = param->l2, a = param->alpha, b = param->beta;
    for (t = 0; t < param->nr_pass; t++) {
    for (FtrlInt chunk_id = 0; chunk_id < nr_chunk; chunk_id++) {

        FtrlChunk chunk = data->chunks[chunk_id];

        chunk.read();

#pragma omp parallel for schedule(guided)
        for (FtrlInt i = 0; i < chunk.l; i++) {

            FtrlFloat y=chunk.labels[i], wTx=0;

            for (FtrlInt s = chunk.nnzs[i]; s < chunk.nnzs[i+1]; s++) {
                Node x = chunk.nodes[s];
                FtrlInt idx = x.idx;
                FtrlFloat val = x.val, zi = z[idx], ni = n[idx];

                if (abs(zi) > l1*f[idx]) {
                    w[idx] = -(zi-(2*(zi>0)-1)*l1*f[idx]) / ((b+sqrt(ni))/a+l2*f[idx]);
                }
                else {
                    w[idx] = 0;
                }
                wTx += w[idx]*val;
            }

            FtrlFloat exp_m, tmp;

            if (wTx*y > 0) {
                exp_m = exp(-y*wTx);
                tmp = exp_m/(1+exp_m);
            }
            else {
                exp_m = exp(y*wTx);
                tmp = 1/(1+exp_m);
            }

            FtrlFloat kappa = -y*tmp;

            for (FtrlInt s = chunk.nnzs[i]; s < chunk.nnzs[i+1]; s++) {
                Node x = chunk.nodes[s];
                FtrlInt idx = x.idx;
                FtrlFloat val = x.val, g = kappa*val, theta=0;
                theta = 1/a*(sqrt(n[idx]+g*g)-sqrt(n[idx]));
                z[idx] += g-theta*w[idx];
                n[idx] += g*g;
            }
        }
        chunk.clear();
    }
    if (param->verbose)
        fun();
    if (!test_data->file_name.empty()) {
    validate();
    }
    print_epoch_info();
    }
}
