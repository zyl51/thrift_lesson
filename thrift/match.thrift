namespace cpp match_server

struct User 
{
    1: i32 userId,
    2: string name,
    3: i32 score
}

service Match 
{
    i32 add_user(1: User user, 2: string info),
    i32 remove_user(1: Uesr user, 2:string info),
}
