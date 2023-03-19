"""Python version of main.cpp"""
import sys
import argparse
import llamacpp


def parse_args_into_params(argv) -> llamacpp.gpt_params:
    """Parses arguments using argparse based on usage information above"""
    parser = argparse.ArgumentParser(description="llama.cpp CLI")
    parser.add_argument("-i", "--interactive", action="store_true", help="run in interactive mode")
    parser.add_argument(
        "-ins", "--instruct",
        action="store_true",
        help="run in 'instruct mode' where the user is prompted to enter a command",
        default=False,
    )
    parser.add_argument(
        "-r",
        "--reverse-prompt",
        type=str,
        help="in interactive mode, poll user input upon seeing PROMPT",
        default="",
    )
    parser.add_argument(
        "--color",
        action="store_true",
        help="colorise output to distinguish prompt and user input from generations",
    )
    parser.add_argument("-s", "--seed", type=int, default=-1, help="RNG seed (default: -1)")
    parser.add_argument(
        "-t",
        "--threads",
        type=int,
        default=4,
        help="number of threads to use during computation (default: 1)",
    )
    parser.add_argument(
        "-p",
        "--prompt",
        type=str,
        help="prompt to start generation with (default: random)",
    )
    parser.add_argument(
        "-f", "--file", type=str, default="", help="prompt file to start generation."
    )
    parser.add_argument(
        "-n", "--n_predict", type=int, default=128, help="number of tokens to predict (default: 128)"
    )
    parser.add_argument("--top_k", type=int, default=40, help="top-k sampling (default: 40)")
    parser.add_argument("--top_p", type=float, default=0.95, help="top-p sampling (default: 0.1)")
    parser.add_argument(
        "--repeat_last_n",
        type=int,
        default=64,
        help="last n tokens to consider for penalize (default: 0)",
    )
    parser.add_argument(
        "--repeat_penalty",
        type=float,
        default=1.30,
        help="penalize repeat sequence of tokens (default: 0.0)",
    )
    parser.add_argument(
        "-c",
        "--ctx_size",
        type=int,
        default=4096,
        help="size of the prompt context (default: 4096)",
    )
    parser.add_argument("--temp", type=float, default=0.8, help="temperature (default: 0.7)")
    parser.add_argument(
        "-b",
        "--batch_size",
        type=int,
        default=8,
        help="batch size for prompt processing (default: 2)",
    )
    parser.add_argument("-m", "--model", type=str, default="./models/7B/ggml-model-q4_0.bin", help="model path (default: )")
    parser.usage = parser.format_help()

    args = parser.parse_args(argv[1:])

    return args


def process_interactive_input(model: llamacpp.PyLLAMA):
    """Process interactive input similar to the C++ version"""

    # Read lines as long as user is entering "\" at the end of the line
    # Pass each line to the model
    while True:
        line = input()
        if line.endswith("\\"):
            line = line[:-1]
            model.update_input(line)
        else:
            model.update_input(line)
            break


def main(args):
    """Main function"""

    # if args.file is specified, read the file and set the prompt to the contents
    if args.file:
        with open(args.file, "r") as f:
            args.prompt = f.read().strip()

    # Add a space in front of the first character to match OG llama tokenizer behavior
    args.prompt = " " + args.prompt

    # Initialize the gpt_params object
    params = llamacpp.gpt_params(
        args.model,
        args.ctx_size,
        args.n_predict,
        args.top_k,
        args.top_p,
        args.temp,
        args.repeat_penalty,
        args.seed,
        args.threads,
        args.repeat_last_n,
        args.batch_size,
    )

    model = llamacpp.PyLLAMA(params)
    model.add_bos()
    model.update_input(args.prompt)
    model.print_startup_stats()
    model.prepare_context()

    inp_pfx = model.tokenize("\n\n### Instruction:\n\n", True)
    inp_sfx = model.tokenize("\n\n### Response:\n\n", False)

    if args.instruct:
        args.interactive = True
        args.antiprompt = "### Instruction:\n\n"

    # Set antiprompt if we are in interactive mode
    if args.antiprompt:
        args.interactive = True
        model.set_antiprompt(args.antiprompt)

    if args.interactive:
        print("== Running in interactive mode. ==")
        print(" - Press Ctrl+C to interject at any time.")
        print(" - Press Return to return control to LLaMa.")
        print(" - If you want to submit another line, end your input in '\\'.")
        print()
        is_interacting = True

    input_noecho = False
    is_finished = False

    while not model.is_finished():
        if model.has_unconsumed_input():
            model.ingest_all_pending_input(not input_noecho)
            # # reset color to default if we there is no pending user input
            # if (!input_noecho && args.use_color) {
            #     printf(ANSI_COLOR_RESET);
            # }
        else:
            text, is_finished = model.infer_text()
            print(text, end="")
            input_noecho = False

        if args.interactive:
            if model.is_antiprompt_present():
                # reverse prompt found
                is_interacting = True
            if is_interacting:
                if args.instruct:
                    model.update_input_tokens(inp_pfx)
                    print("\n> ", end="")

                process_interactive_input(model)

                if args.instruct:
                    model.update_input_tokens(inp_sfx)

                input_noecho = True
                is_interacting = False
        
        # end of text token was found
        if is_finished:
            if args.interactive:
                is_interacting = True
            else:
                print(" [end of text]")
                break
        
        if args.interactive and model.is_finished():
            model.reset_remaining_tokens()
            is_interacting = True

    return 0


def run():
    # Parse params into a gpt_params object
    args = parse_args_into_params(sys.argv)
    return main(args)

if __name__ == "__main__":
    sys.exit(run())
