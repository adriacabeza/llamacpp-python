#include "ggml.h"
#include "llama.h"
#include "utils.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

class Tokenizer {
    gpt_vocab vocab{};
public:
    Tokenizer(const gpt_vocab& vocabIn): vocab(vocabIn) {}
    std::vector<gpt_vocab::id> tokenize(const std::string & text, bool bos) {
        return llama_tokenize(vocab, text, bos);
    }
    std::string detokenize(const std::vector<gpt_vocab::id>& ids) {
        std::string output = "";
        for (auto id: ids) {
            output += vocab.id_to_token.at(id);
        }
        return output;
    }
    std::string detokenize(const gpt_vocab::id& id) {
        return vocab.id_to_token.at(id);
    }
};

class PyLLAMA {
    llama_context* ctx_ptr = nullptr;
    gpt_params params{};
    std::vector<gpt_vocab::id> antiprompt_inp{};
public:
    PyLLAMA(gpt_params params): params(params) {
        if (params.seed < 0) {
            params.seed = time(NULL);
        }
        ctx_ptr = llama_init_from_params(params);
        if(ctx_ptr == nullptr) {
            fprintf(stderr, "%s: failed to initialize llama context", __func__);
            throw std::runtime_error("Failed to load model");
        }
    }
    ~PyLLAMA() {
        if (ctx_ptr != nullptr)
        {
            llama_free_context(ctx_ptr);
        }
    }
    Tokenizer get_tokenizer() {
        return Tokenizer(vocab());
    }
    // Pass through all functions for llama_context
    const gpt_vocab & vocab() const {
        return llama_context_get_vocab(*ctx_ptr);
    }
    std::vector<gpt_vocab::id> tokenize(const std::string & text, bool bos) {
        return llama_tokenize(vocab(), text, bos);
    }
    void add_bos() {
        llama_add_bos(*ctx_ptr);
    }
    void prepare_context() {
        if(!llama_prepare_context(*ctx_ptr))
        {
            throw std::runtime_error("Failed to prepare context");
        }
    }
    void update_input(const std::string& text) {
        llama_update_input(*ctx_ptr, text);
    }
    void update_input_tokens(const std::vector<gpt_vocab::id>& ids) {
        llama_update_input(*ctx_ptr, ids);
    }
    bool is_finished() {
        return llama_context_is_finished(*ctx_ptr);
    }
    bool has_unconsumed_input() {
        return llama_has_unconsumed_input(*ctx_ptr);
    }
    bool ingest_all_pending_input(bool print_tokens) {
        return llama_ingest_all_pending_input(*ctx_ptr, print_tokens);
    }
    bool infer(std::string& output, bool& is_finished) {
        return llama_infer(*ctx_ptr, output, is_finished);
    }
    bool infer(gpt_vocab::id& output) {
        return llama_infer(*ctx_ptr, output);
    }
    std::vector<gpt_vocab::id> get_antiprompt() {
        return antiprompt_inp;
    }
    void set_antiprompt(const std::string & antiprompt) {
        antiprompt_inp = llama_tokenize(vocab(), antiprompt, false);
    }
    bool is_antiprompt_present() {
        if (antiprompt_inp.empty()) {
            return false;
        }else{
            return llama_is_anti_prompt_present(*ctx_ptr, antiprompt_inp);
        }
    }
    void reset_remaining_tokens() {
        llama_reset_remaining_tokens(*ctx_ptr);
    }
    void print_startup_stats() {
        llama_print_startup_stats(*ctx_ptr);
    }
    void print_end_stats() {
        llama_print_end_stats(*ctx_ptr);
    }
};

// Write python bindings for gpt_params
gpt_params init_params(
    const std::string& model,
    int32_t n_ctx,
    int32_t n_predict,
    int32_t top_k,
    float top_p,
    float temp,
    float repeat_penalty,
    int32_t seed,
    int32_t n_threads,
    int32_t repeat_last_n,
    int32_t n_batch
) {
    gpt_params params{};
    params.model = model;
    params.n_predict = n_predict;
    params.n_ctx = n_ctx;
    params.top_k = top_k;
    params.top_p = top_p;
    params.temp = temp;
    params.repeat_penalty = repeat_penalty;
    params.seed = seed;
    params.n_threads = n_threads;
    params.repeat_last_n = repeat_last_n;
    params.n_batch = n_batch;
    return params;
}

// std::vector<gpt_vocab::id> llama_tokenize(const gpt_vocab & vocab, const std::string & text, bool bos);
PYBIND11_MODULE(llamacpp, m) {
    m.doc() = "Python bindings for C++ implementation of the LLaMA language model";
    py::class_<gpt_params>(m, "gpt_params")
        .def(py::init<>(&init_params), "Initialize gpt_params",
             py::arg("model"),
             py::arg("n_ctx"),
             py::arg("n_predict"),
             py::arg("top_k"),
             py::arg("top_p"),
             py::arg("temp"),
             py::arg("repeat_penalty"),
             py::arg("seed"),
             py::arg("n_threads"),
             py::arg("repeat_last_n"),
             py::arg("n_batch"))
        .def_readwrite("model", &gpt_params::model)
        .def_readwrite("n_predict", &gpt_params::n_predict)
        .def_readwrite("n_ctx", &gpt_params::n_ctx)
        .def_readwrite("top_k", &gpt_params::top_k)
        .def_readwrite("top_p", &gpt_params::top_p)
        .def_readwrite("temp", &gpt_params::temp)
        .def_readwrite("repeat_penalty", &gpt_params::repeat_penalty)
        .def_readwrite("seed", &gpt_params::seed)
        .def_readwrite("n_threads", &gpt_params::n_threads)
        .def_readwrite("repeat_last_n", &gpt_params::repeat_last_n)
        .def_readwrite("n_batch", &gpt_params::n_batch);

    /* Wrapper for PyLLAMA */
    py::class_<PyLLAMA>(m, "PyLLAMA")
        .def(py::init<gpt_params>())
        .def("get_tokenizer", &PyLLAMA::get_tokenizer, "Get the tokenizer")
        .def("tokenize", &PyLLAMA::tokenize, "Tokenize text")
        .def("prepare_context", &PyLLAMA::prepare_context, "Prepare the LLaMA context")
        .def("add_bos", &PyLLAMA::add_bos, "Add a BOS token to the input")
        .def("update_input", &PyLLAMA::update_input, "Update input as text")
        .def("update_input_tokens", &PyLLAMA::update_input_tokens, "Update input tokens")
        .def("is_finished", &PyLLAMA::is_finished, "Check if the model is finished")
        .def("has_unconsumed_input", &PyLLAMA::has_unconsumed_input, "Check if the model has unconsumed input")
        // ingest_all_pending_input. Does not print to stdout by default
        .def("ingest_all_pending_input", &PyLLAMA::ingest_all_pending_input, "Ingest all pending input",
            py::arg("print_tokens") = false)
        .def("infer_text", [](PyLLAMA &llama){
            bool is_end_of_text;
            std::string output;

            bool ret = llama.infer(output, is_end_of_text);
            if(!ret)
            {
                throw std::runtime_error("Failed to run inference");
            }
            return std::make_tuple(output, is_end_of_text);
        }, "Infer the next token and return it as a string")
        .def("infer_token", [](PyLLAMA &llama){
            gpt_vocab::id output{};

            bool ret = llama.infer(output);
            if(!ret)
            {
                throw std::runtime_error("Failed to run inference");
            }
            return output;
        }, "Infer the next token")
        .def("set_antiprompt", &PyLLAMA::set_antiprompt, "Set antiprompt")
        .def("is_antiprompt_present", &PyLLAMA::is_antiprompt_present, "Check if antiprompt is present")
        .def("print_startup_stats", &PyLLAMA::print_startup_stats, "Print startup stats")
        .def("print_end_stats", &PyLLAMA::print_end_stats, "Print end stats")
        .def("reset_remaining_tokens", &PyLLAMA::reset_remaining_tokens, "Reset remaining tokens")
        .def_property("antiprompt", &PyLLAMA::get_antiprompt, &PyLLAMA::set_antiprompt, "Antiprompt")
        ;

    /* Wrapper for Tokenizer */
    py::class_<Tokenizer>(m, "Tokenizer")
        .def("tokenize", &Tokenizer::tokenize, "Tokenize text", py::arg("text"), py::arg("add_bos") = false)
        .def("detokenize", py::overload_cast<const std::vector<gpt_vocab::id>&>(&Tokenizer::detokenize), "Detokenize text")
        .def("detokenize", py::overload_cast<const gpt_vocab::id&>(&Tokenizer::detokenize), "Detokenize single token");

    /* Wrapper for llama_model_quantize */
    m.def("llama_model_quantize", &llama_model_quantize, "Quantize the LLaMA model");
}
