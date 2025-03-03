import array
import llamacpp
import pytest


@pytest.fixture(scope="session")
def llama_context():
    params = llamacpp.LlamaContextParams()
    params.seed = 19472
    return llamacpp.LlamaContext("../models/7B/ggml-model-f16.bin", params)


def test_str_to_token(llama_context):
    prompt = "Hello World"
    prompt_tokens = llama_context.str_to_token(prompt, True)
    assert prompt_tokens == [1, 10994, 2787]


def test_token_to_str(llama_context):
    tokens = [1, 10994, 2787]
    text = ''.join([llama_context.token_to_str(token) for token in tokens])
    assert text == "Hello World"


def test_eval(llama_context):
    embd_inp = llama_context.str_to_token(" Llama is", True)
    n_past, n_remain, n_consumed  = 0, 8, 0
    embd = []

    output = ''
    while n_remain:
        if len(embd):
            llama_context.eval(array.array('i', embd), len(embd), n_past, 1)
        n_past += len(embd)
        embd.clear()

        if len(embd_inp) <= n_consumed:
            # sample
            top_k = 40
            top_p = 0.95
            temp = 0.8
            repeat_last_n = 64

            # sending an empty array for the last n tokens
            id = llama_context.sample_top_p_top_k(array.array('i', []), top_k, top_p, temp, repeat_last_n)
            # add it to the context
            embd.append(id)
            # decrement remaining sampling budget
            n_remain -= 1
        else:
            # has unconsumed input
            while len(embd_inp) > n_consumed:
                # update_input
                embd.append(embd_inp[n_consumed])
                n_consumed += 1

        output += ''.join([llama_context.token_to_str(id) for id in embd])
    assert output == " Llama is the newest member of our farm family"
