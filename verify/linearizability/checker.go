package main

import (
    "encoding/json"
    "fmt"
    "os"
    "time"

    "github.com/anishathalye/porcupine"
)

type rawOperation struct {
    ID        uint64 `json:"id"`
    Client    int    `json:"client"`
    Operation string `json:"operation"`
    Value     int64  `json:"value"`
    Invoke    int64  `json:"invoke"`
    Complete  int64  `json:"complete"`
    Success   bool   `json:"success"`
    Result    int64  `json:"result"`
}

type historyFile struct {
    Capacity   int            `json:"capacity"`
    Seed       uint64         `json:"seed"`
    OpsPerSide uint64         `json:"ops_per_side"`
    Profile    string         `json:"profile"`
    Operations []rawOperation `json:"operations"`
}

type input struct {
    Operation string
    Value     int64
}

type output struct {
    Success bool
    Value   int64
}

type state struct {
    Capacity int
    Items    []int64
}

func model(capacity int) porcupine.Model {
    return porcupine.Model{
        Init: func() interface{} { return state{Capacity: capacity} },
        Step: func(rawState, rawInput, rawOutput interface{}) (bool, interface{}) {
            s := rawState.(state)
            in := rawInput.(input)
            out := rawOutput.(output)
            next := state{Capacity: s.Capacity, Items: append([]int64(nil), s.Items...)}
            switch in.Operation {
            case "push":
                expectedSuccess := len(s.Items) < s.Capacity
                if out.Success != expectedSuccess {
                    return false, s
                }
                if expectedSuccess {
                    next.Items = append(next.Items, in.Value)
                }
                return true, next
            case "pop":
                expectedSuccess := len(s.Items) > 0
                if out.Success != expectedSuccess {
                    return false, s
                }
                if expectedSuccess {
                    if out.Value != s.Items[0] {
                        return false, s
                    }
                    next.Items = append([]int64(nil), s.Items[1:]...)
                }
                return true, next
            default:
                return false, s
            }
        },
        Equal: func(a, b interface{}) bool {
            x, y := a.(state), b.(state)
            if x.Capacity != y.Capacity || len(x.Items) != len(y.Items) {
                return false
            }
            for i := range x.Items {
                if x.Items[i] != y.Items[i] {
                    return false
                }
            }
            return true
        },
        DescribeOperation: func(i, o interface{}) string {
            in, out := i.(input), o.(output)
            if in.Operation == "push" {
                return fmt.Sprintf("push(%d) -> %v", in.Value, out.Success)
            }
            if out.Success {
                return fmt.Sprintf("pop() -> %d", out.Value)
            }
            return "pop() -> empty"
        },
        DescribeState: func(s interface{}) string { return fmt.Sprintf("%v", s.(state).Items) },
    }
}

func main() {
    if len(os.Args) < 2 {
        fmt.Fprintln(os.Stderr, "usage: checker <history.json> [visualization.html]")
        os.Exit(2)
    }
    data, err := os.ReadFile(os.Args[1])
    if err != nil {
        panic(err)
    }
    var h historyFile
    if err := json.Unmarshal(data, &h); err != nil {
        panic(err)
    }

    ops := make([]porcupine.Operation, 0, len(h.Operations))
    for _, op := range h.Operations {
        ops = append(ops, porcupine.Operation{
            ClientId: op.Client,
            Input:    input{Operation: op.Operation, Value: op.Value},
            Call:     op.Invoke,
            Output:   output{Success: op.Success, Value: op.Result},
            Return:   op.Complete,
        })
    }

    m := model(h.Capacity)
    result, info := porcupine.CheckOperationsVerbose(m, ops, 10*time.Second)
    if len(os.Args) >= 3 {
        if err := porcupine.VisualizePath(m, info, os.Args[2]); err != nil {
            panic(err)
        }
    }

    prefix := fmt.Sprintf(
        "capacity=%d profile=%s seed=%d ops_per_side=%d operations=%d",
        h.Capacity, h.Profile, h.Seed, h.OpsPerSide, len(ops),
    )
    switch result {
    case porcupine.Ok:
        fmt.Printf("PASS %s\n", prefix)
    case porcupine.Illegal:
        fmt.Printf("FAIL %s\n", prefix)
        os.Exit(1)
    default:
        fmt.Printf("UNKNOWN %s\n", prefix)
        os.Exit(3)
    }
}
